## FPGA Evaluation

Three scripts are provided to evaluate correctness and performance of the application on FPGA. To use the scripts, compile the application with `-DDEBUG` on desktop and paste output in `desktop.log`, and compile the application with `-DDEBUG -DLOG_PERF` on fpga and paste output in `fpga.log`.

`compare.py` compares raw features for discrepancy between desktop and FPGA, and outputs lines where precision drifting exceeds tolerance, as well as overall maximum absolute and relative drift, and mean and std-dev drift.

`graph.py` compares raw features and generates heatmap of precision drifting on FPGA.

`perf.py` computes the mean and std-dev of clock cycles used when processing each window of input data, as well as line graph of the trend of clock cycles in one specific input dataset.
