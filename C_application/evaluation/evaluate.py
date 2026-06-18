"""
Evaluation pipeline for the Cough-E C application.

Pipeline per recording:
  1. Generate C header files (via transform_dataset.py)
  2. Update main.h includes to point to the generated headers
  3. Compile the C application
  4. Run the C application and capture output
  5. Parse COUGH_SEG lines to get detected cough segment boundaries
  6. Compare with ground truth using event-based scoring (timescoring)

Usage:
    python C_application/evaluation/evaluate.py                                     # full pipeline, all subjects
    python C_application/evaluation/evaluate.py full --subjects 14287 14342         # specific subjects
    python C_application/evaluation/evaluate.py full --dataset_path /path/to/data   # custom dataset path
    python C_application/evaluation/evaluate.py aggregate --csv C_application/evaluation/results.csv  # re-aggregate from CSV
    python C_application/evaluation/evaluate.py full -j 8                           # run with 8 parallel threads
"""

import argparse
import concurrent.futures
import csv
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from timescoring.annotations import Annotation
from timescoring import scoring

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from transform_dataset import (
    transform_recording, transform_all, make_recording_suffix,
    AUDIO_FS_TARGET, IMU_FS, FS_IMU,
    SOUNDS, NOISES, MOVEMENTS, TRIALS,
)


# ──────────────────────────────────────────────
#  Constants
# ──────────────────────────────────────────────

# Scoring parameters (matching ML_methodology/config/scoring/default.yaml)
TOLERANCE_START = 0.25
TOLERANCE_END = 0.25
MIN_COUGH_DURATION = 0.1
MAX_EVENT_DURATION = 0.6
MIN_DURATION_BTWN_EVENTS = 0
MIN_OVERLAP = MIN_COUGH_DURATION / 0.8  # 0.125

# Paths (relative to repo root)
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
C_APP_DIR = os.path.join(REPO_ROOT, "C_application")
INPUT_DATA_DIR = os.path.join(C_APP_DIR, "input_data")

DEFAULT_DATASET_PATH = os.path.join(REPO_ROOT, "Datasets", "full_dataset_test")

CSV_FIELDNAMES = [
    "subject", "trial", "movement", "noise", "sound",
    "tp_evt", "fp_evt", "fn_evt", "se_evt", "ppv_evt", "f1_evt",
    "duration",
]

NUM_FMT = "float" # or, "unum-posit"


# ──────────────────────────────────────────────
#  Isolated Workspace Management
# ──────────────────────────────────────────────

def update_main_h(ws_path, audio_relpath, imu_relpath, bio_relpath):
    """Replace the 3 input data #include lines in a workspace's main.h."""
    main_h_path = os.path.join(ws_path, "main.h")
    with open(main_h_path, 'r') as f:
        content = f.read()

    content = re.sub(r'#include <input_data/.*audio_input.*\.h>',
                     f'#include <input_data/{audio_relpath}>', content)
    content = re.sub(r'#include <input_data/.*imu_input.*\.h>',
                     f'#include <input_data/{imu_relpath}>', content)
    content = re.sub(r'#include <input_data/.*bio_input.*\.h>',
                     f'#include <input_data/{bio_relpath}>', content)

    with open(main_h_path, 'w') as f:
        f.write(content)


def compile_c_app(ws_path):
    """Compile the C application in the isolated workspace. Returns True on success."""
    result = None
    if NUM_FMT == "float":
        result = subprocess.run(["make", "-C", ws_path, "CFLAGS=-DEVALUATION_MODE"],
                                capture_output=True, text=True)
    elif NUM_FMT == "unum-posit":
        result = subprocess.run(["make", "-C", ws_path, "CC=g++", "CFLAGS=-DEVALUATION_MODE -DUSE_UNUM_POSIT"],
                                capture_output=True, text=True)
    else:
        print("  Invalid --num-fmt argument")
        return False

    if result.returncode != 0:
        print(f"  Compilation failed in {ws_path}: {result.stderr}")
        return False
    return True


def run_c_app(ws_path):
    """Run the compiled C application from the workspace and return stdout."""
    executable = os.path.join(ws_path, "build", "cough-e")
    try:
        result = subprocess.run([executable], capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired:
        print(f"    WARNING: C app timed out after 120s in {ws_path} (possible stuck)", flush=True)
        return ""
    return result.stdout


# ──────────────────────────────────────────────
#  Output parsing
# ──────────────────────────────────────────────

def parse_c_output(output, audio_fs=AUDIO_FS_TARGET):
    """
    Parse C application output to extract detected cough segments.
    """
    periods = []
    current_period_segs = []

    for line in output.strip().split('\n'):
        seg_match = re.match(r'COUGH_SEG:\s+(\d+)\s+(\d+)', line)
        peaks_match = re.match(r'N_PEAKS FINAL:\s+(\d+)', line)

        if seg_match:
            start_sample = int(seg_match.group(1))
            end_sample = int(seg_match.group(2))
            current_period_segs.append((start_sample, end_sample))
        elif peaks_match:
            periods.append(current_period_segs)
            current_period_segs = []

    seen_signatures = set()
    first_pass_periods = []
    for period_segs in periods:
        sig = tuple(period_segs)
        if sig in seen_signatures and len(sig) > 0:
            break
        seen_signatures.add(sig)
        first_pass_periods.append(period_segs)

    segments = []
    seen_segments = set()
    for period_segs in first_pass_periods:
        for start_sample, end_sample in period_segs:
            key = (start_sample, end_sample)
            if key not in seen_segments:
                seen_segments.add(key)
                segments.append((start_sample / audio_fs, end_sample / audio_fs))

    return segments


# ──────────────────────────────────────────────
#  Ground truth & binary masks
# ──────────────────────────────────────────────

def load_ground_truth(dataset_path, subj_id, trial, mov, noise, sound):
    """Load ground truth cough events. Returns empty list for non-cough sounds."""
    if sound != "cough":
        return []
    gt_path = os.path.join(dataset_path, subj_id,
                           f'trial_{trial}', f'mov_{mov}',
                           f'background_noise_{noise}', sound,
                           'ground_truth.json')
    if not os.path.exists(gt_path):
        return []
    with open(gt_path, 'r') as f:
        gt = json.load(f)
    return list(zip(gt["start_times"], gt["end_times"]))


def get_recording_duration(dataset_path, subj_id, trial, mov, noise, sound):
    """Get recording duration in seconds from IMU CSV line count."""
    imu_path = os.path.join(dataset_path, subj_id,
                            f'trial_{trial}', f'mov_{mov}',
                            f'background_noise_{noise}', sound,
                            'imu.csv')
    if os.path.exists(imu_path):
        with open(imu_path, 'r') as f:
            n_lines = sum(1 for _ in f) - 1
        return n_lines / IMU_FS
    return 0.0


def create_binary_mask(events, duration):
    """Create a binary mask at FS_IMU resolution from a list of (start, end) events."""
    n_samples = int(round(duration * FS_IMU))
    mask = np.zeros(n_samples)
    for start, end in events:
        s = min(int(round(start * FS_IMU)), n_samples)
        e = min(int(round(end * FS_IMU)), n_samples)
        mask[s:e] = 1
        if 0 < s < n_samples:
            mask[s - 1] = 0
    return mask


# ──────────────────────────────────────────────
#  Scoring
# ──────────────────────────────────────────────

def score_recording(gt_events, pred_events, duration):
    """Compute event-based scoring using timescoring.EventScoring."""
    gt_mask = create_binary_mask(gt_events, duration)
    pred_mask = create_binary_mask(pred_events, duration)

    labels = Annotation(gt_mask, FS_IMU)
    pred = Annotation(pred_mask, FS_IMU)

    param = scoring.EventScoring.Parameters(
        TOLERANCE_START, TOLERANCE_END, MIN_OVERLAP,
        MAX_EVENT_DURATION, MIN_DURATION_BTWN_EVENTS
    )
    scores = scoring.EventScoring(labels, pred, param)

    return {
        "tp_evt": scores.tp,
        "fp_evt": scores.fp,
        "fn_evt": scores.refTrue - scores.tp,
        "se_evt": scores.sensitivity,
        "ppv_evt": scores.precision,
        "f1_evt": scores.f1,
    }


# ──────────────────────────────────────────────
#  Per-recording evaluation
# ──────────────────────────────────────────────

def evaluate_recording_isolated(subj_id, trial, mov, noise, sound, dataset_path, ws_path):
    """Full pipeline for a single recording bounded to an isolated workspace."""
    input_data_dir = os.path.join(ws_path, "input_data")

    result = transform_recording(subj_id, trial, mov, noise, sound,
                                 dataset_path, input_data_dir)
    if result is None:
        return None

    suffix, audio_relpath, imu_relpath, bio_relpath = result
    update_main_h(ws_path, audio_relpath, imu_relpath, bio_relpath)

    if not compile_c_app(ws_path):
        return None

    output = run_c_app(ws_path)
    if not output:
       	print(f"  [WARN] No output at {ws_path} (likely segfault).")
       	shutil.copytree(ws_path, ws_path + '_WARN')
    pred_segments = parse_c_output(output)
    gt_events = load_ground_truth(dataset_path, subj_id, trial, mov, noise, sound)
    duration = get_recording_duration(dataset_path, subj_id, trial, mov, noise, sound)

    scores = score_recording(gt_events, pred_segments, duration)
    scores.update({
        "subject": subj_id,
        "trial": trial,
        "movement": mov,
        "noise": noise,
        "sound": sound,
        "duration": duration,
    })

    return scores


def _worker_task(task_args, dataset_path, workspaces):
    """Pulls an available workspace, runs eval, then releases it back to the queue."""
    subj_id, trial, mov, noise, sound = task_args
    ws_path = workspaces.get()
    try:
        return evaluate_recording_isolated(
            subj_id, trial, mov, noise, sound,
            dataset_path, ws_path
        )
    finally:
        workspaces.put(ws_path)


def evaluate_subjects(dataset_path, subjects=None,
                      trials=TRIALS, movements=MOVEMENTS,
                      noises=NOISES, sounds=SOUNDS, jobs=1):
    """Evaluate all recordings via a thread pool with isolated workspaces."""
    if subjects is None:
        subjects = sorted([
            s for s in os.listdir(dataset_path)
            if os.path.isdir(os.path.join(dataset_path, s))
        ])

    all_results = []
    temp_dirs = []
    workspaces = queue.Queue()

    print(f"\nSetting up {jobs} isolated C_application workspaces...")
    try:
        # Initialize pool directories to prevent make/file collisions
        for i in range(jobs):
            ws_path = tempfile.mkdtemp(prefix=f"cough_e_eval_worker_{i}_")

            # Copy C_APP_DIR but ignore dotfiles, evaluation, and input_data
            shutil.copytree(
                C_APP_DIR,
                ws_path,
                dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('.*', 'evaluation', 'input_data')
            )

            # Re-create the input_data directory wrapper in the worker
            ws_input_data = os.path.join(ws_path, "input_data")
            os.makedirs(ws_input_data, exist_ok=True)

            # Symlink existing static directories back to the main C_application/input_data
            main_input_data = os.path.join(C_APP_DIR, "input_data")
            if os.path.exists(main_input_data):
                for item in os.listdir(main_input_data):
                    main_item_path = os.path.join(main_input_data, item)
                    if os.path.isdir(main_item_path) and not item.startswith('.'):
                        ws_item_path = os.path.join(ws_input_data, item)
                        os.symlink(main_item_path, ws_item_path)

            workspaces.put(ws_path)
            temp_dirs.append(ws_path)

        tasks = []
        for subj_id in subjects:
            for trial in trials:
                for mov in movements:
                    for noise in noises:
                        for sound in sounds:
                            tasks.append((subj_id, trial, mov, noise, sound))

        print(f"Beginning evaluation of {len(tasks)} recordings...")
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as executor:
            future_to_task = {
                executor.submit(_worker_task, t, dataset_path, workspaces): t
                for t in tasks
            }

            for future in concurrent.futures.as_completed(future_to_task):
                task_args = future_to_task[future]
                subj_id, trial, mov, noise, sound = task_args
                rec_id = f"t{trial}_{mov}_{noise}_{sound}"

                try:
                    result = future.result()
                    if result is not None:
                        all_results.append(result)
                        print(f"  [{subj_id}] {rec_id}: TP_evt={result['tp_evt']} "
                              f"FP_evt={result['fp_evt']} FN_evt={result['fn_evt']}")
                except Exception as exc:
                    print(f"  [{subj_id}] {rec_id} generated an exception: {exc}")

    finally:
        print("\nCleaning up workspaces...")
        for d in temp_dirs:
            shutil.rmtree(d, ignore_errors=True)

    return all_results


# ──────────────────────────────────────────────
#  Aggregation
# ──────────────────────────────────────────────

def compute_aggregate_metrics(results):
    """Compute aggregate event-based metrics across all recordings."""
    total_duration_hrs = sum(r["duration"] for r in results) / 3600.0

    tp = sum(r["tp_evt"] for r in results)
    fp = sum(r["fp_evt"] for r in results)
    fn = sum(r["fn_evt"] for r in results)

    se = tp / (tp + fn) if (tp + fn) > 0 else 0.0
    pr = tp / (tp + fp) if (tp + fp) > 0 else 0.0
    f1 = 2 * se * pr / (se + pr) if (se + pr) > 0 else 0.0
    fphr = fp / total_duration_hrs if total_duration_hrs > 0 else 0.0

    return {
        "se_evt": se, "pr_evt": pr, "f1_evt": f1, "fphr_evt": fphr,
        "tp_evt": tp, "fp_evt": fp, "fn_evt": fn,
        "total_recordings": len(results),
        "total_duration_hrs": total_duration_hrs,
    }


# ──────────────────────────────────────────────
#  Output: CSV, JSON, terminal
# ──────────────────────────────────────────────

def save_results_csv(results, output_path):
    """Save per-recording results to CSV."""
    with open(output_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDNAMES)
        writer.writeheader()
        for r in results:
            writer.writerow({k: r[k] for k in CSV_FIELDNAMES})
    print(f"Results CSV saved to {output_path}")


def load_results_csv(csv_path):
    """Load per-recording results from CSV."""
    results = []
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            for k in row:
                if k not in ("subject", "trial", "movement", "noise", "sound"):
                    row[k] = float(row[k])
            results.append(row)
    return results


def save_summary_json(aggregate, output_path, per_subject=None):
    """Save aggregate metrics to JSON."""
    class NumpyEncoder(json.JSONEncoder):
        def default(self, obj):
            if isinstance(obj, (np.integer,)):
                return int(obj)
            if isinstance(obj, (np.floating,)):
                return float(obj)
            return super().default(obj)

    summary = {
        "timestamp": datetime.now().isoformat(),
        "overall": {
            "event_based": {
                "SE": round(aggregate["se_evt"], 4),
                "PR": round(aggregate["pr_evt"], 4),
                "F1": round(aggregate["f1_evt"], 4),
                "FP_hr": round(aggregate["fphr_evt"], 1),
                "TP": aggregate["tp_evt"],
                "FP": aggregate["fp_evt"],
                "FN": aggregate["fn_evt"],
            },
            "total_recordings": aggregate["total_recordings"],
            "total_duration_hrs": round(aggregate["total_duration_hrs"], 3),
        },
    }
    if per_subject:
        summary["per_subject"] = per_subject

    with open(output_path, 'w') as f:
        json.dump(summary, f, indent=2, cls=NumpyEncoder)
    print(f"Summary JSON saved to {output_path}")


def build_per_subject_json(per_subject):
    """Build per-subject metrics dict for JSON output."""
    result = {}
    for subj, a in per_subject.items():
        result[subj] = {
            "event_based": {
                "SE": round(a["se_evt"], 4),
                "PR": round(a["pr_evt"], 4),
                "F1": round(a["f1_evt"], 4),
                "FP_hr": round(a["fphr_evt"], 1),
                "TP": a["tp_evt"],
                "FP": a["fp_evt"],
                "FN": a["fn_evt"],
            },
        }
    return result


def print_results(results, aggregate):
    """Print per-subject and overall results to terminal."""
    print("\n" + "=" * 70)
    print("EVALUATION RESULTS")
    print("=" * 70)

    subjects = sorted(set(r["subject"] for r in results))
    per_subject = {}
    for subj in subjects:
        subj_results = [r for r in results if r["subject"] == subj]
        a = compute_aggregate_metrics(subj_results)
        per_subject[subj] = a
        print(f"\nSubject {subj}:")
        print(f"  SE={a['se_evt']:.3f}  PR={a['pr_evt']:.3f}  "
              f"F1={a['f1_evt']:.3f}  FP/hr={a['fphr_evt']:.1f}  "
              f"(TP={a['tp_evt']} FP={a['fp_evt']} FN={a['fn_evt']})")

    print(f"\n{'=' * 70}")
    print(f"OVERALL ({aggregate['total_recordings']} recordings, "
          f"{aggregate['total_duration_hrs']:.3f} hrs)")
    print(f"{'=' * 70}")
    print(f"  SE    = {aggregate['se_evt']:.4f}")
    print(f"  PR    = {aggregate['pr_evt']:.4f}")
    print(f"  F1    = {aggregate['f1_evt']:.4f}")
    print(f"  FP/hr = {aggregate['fphr_evt']:.1f}")
    print(f"  TP={aggregate['tp_evt']}  FP={aggregate['fp_evt']}  FN={aggregate['fn_evt']}")
    print("=" * 70)

    return per_subject


# ──────────────────────────────────────────────
#  CLI subcommands
# ──────────────────────────────────────────────

def cmd_transform(args):
    """Generate dataset only."""
    transform_all(args.dataset_path, INPUT_DATA_DIR, args.subjects)


def cmd_run(args):
    """Run evaluation (transforms dataset if needed)."""
    output_dir = args.output_dir or os.path.dirname(__file__)
    os.makedirs(output_dir, exist_ok=True)

    results = evaluate_subjects(
        args.dataset_path,
        subjects=args.subjects,
        sounds=args.sounds,
        noises=args.noises,
        jobs=args.jobs
    )

    if not results:
        print("No recordings processed. Check dataset path and subject IDs.")
        sys.exit(1)

    aggregate = compute_aggregate_metrics(results)
    per_subject = print_results(results, aggregate)

    save_results_csv(results, os.path.join(output_dir, "results.csv"))
    save_summary_json(aggregate, os.path.join(output_dir, "summary.json"),
                      build_per_subject_json(per_subject))


def cmd_aggregate(args):
    """Compute aggregate metrics from an existing CSV."""
    results = load_results_csv(args.csv)
    aggregate = compute_aggregate_metrics(results)
    per_subject = print_results(results, aggregate)

    output_dir = os.path.dirname(args.csv) or "."
    save_summary_json(aggregate, os.path.join(output_dir, "summary.json"),
                      build_per_subject_json(per_subject))


def cmd_full(args):
    """Generate dataset + run evaluation."""
    print("Step 1/2: Transforming dataset...")
    transform_all(args.dataset_path, INPUT_DATA_DIR, args.subjects)
    print("\nStep 2/2: Running evaluation...")
    cmd_run(args)


def main():
    global NUM_FMT

    parser = argparse.ArgumentParser(
        description="Evaluate Cough-E C application against full_dataset_test")
    subparsers = parser.add_subparsers(dest="command")

    def add_common_args(p):
        p.add_argument("--num-fmt", type=str, default=NUM_FMT)
        p.add_argument("--dataset_path", type=str, default=DEFAULT_DATASET_PATH,
                        help=f"Path to full_dataset_test (default: {DEFAULT_DATASET_PATH})")
        p.add_argument("--subjects", nargs="+", type=str, default=None,
                        help="Specific subject IDs (default: all)")
        p.add_argument("--sounds", nargs="+", type=str, default=SOUNDS)
        p.add_argument("--noises", nargs="+", type=str, default=NOISES)
        p.add_argument("--output_dir", type=str, default=None,
                        help="Output directory (default: evaluation/)")
        p.add_argument("-j", "--jobs", type=int, default=1,
                        help="Number of threads for parallel evaluation (default: 1)")

    p_transform = subparsers.add_parser("transform", help="Generate C headers from dataset")
    add_common_args(p_transform)
    p_transform.set_defaults(func=cmd_transform)

    p_run = subparsers.add_parser("run", help="Run evaluation")
    add_common_args(p_run)
    p_run.set_defaults(func=cmd_run)

    p_agg = subparsers.add_parser("aggregate", help="Compute metrics from existing CSV")
    p_agg.add_argument("--csv", type=str, required=True, help="Path to results CSV")
    p_agg.set_defaults(func=cmd_aggregate)

    p_full = subparsers.add_parser("full", help="Transform dataset + run evaluation")
    add_common_args(p_full)
    p_full.set_defaults(func=cmd_full)

    args = parser.parse_args()
    NUM_FMT = args.num_fmt

    if args.command is None:
        args = parser.parse_args(["full"])

    args.func(args)


if __name__ == "__main__":
    main()
