### How To Run
1. make
2. vcpu_scheduler 2

### The Algorithm Explained
There are two algorithms implemented, and one is designed
1. rebalance (implemented but not used)
2. rebalanceBySorting (used to generate log files)
3. rebalanceByBacktracking (not implemented)

#### rebalance
This algorithm rebalances pCPU workload by moving workload from the most busy pCPU to most free pCPU, one at a time. The following is an example.

| round |  behavior  |
| ----------- | ----------- |
| 1 | move workload of domain 1 from pCPU 0 to pCPU 2 |
| 2 | move workload of domain 2 from pCPU 0 to pCPU 1 |
| 3 | move workload of domain 3 from pCPU 0 to pCPU 0 |
|  ...  | keep moving until balance ... |

Issue with this algorithm is that it always move workload from the busiest one to freest one, and as a result, it is not guarantee to have a deterministic result. For example, in round 1, pCPU 1 has the most workload and some of them is moved to pCPU 2. In next round, pCPU 2 has the most workload and again some of them is moved back to pCPU 1. Workload is been moved back and forth between pCPU 1 and pCPU 2.

#### rebalance by sorting
This algorithm rebalances pCPU workload by sorting all domains' workload. It then iterates through the sorted workload list with two pointers pointing to the least workload and highest workload. It then assigns those two workload to the same pCPU, put the pointer forward and the other backward, and repeat the assigning till no workload left.

| round | behavior (assume 4 pCPU) |
| ----------- | ----------- |
| 1 | workload: [1, 2, 3, 4, 5, 6, 7, 8], assign 1 and 8 to pCPU 0 (total: 9) |
| 2 | workload: [2, 3, 4, 5, 6, 7], assign 2 and 7 to pCPU 1 (total: 9)  |
| 3 | workload: [3, 4, 5, 6], assign 3 and 6 to pCPU 2 (total: 9)  |
| 4 | workload: [4, 5], assign 4 and 5 to pCPU 4 (total: 9)  |

Issue with this algorithm: It is also not guaranteed to have the optimal solution. With workload balanced system (like all test cases in this project), this algorithm has a great result.

#### rebalance by back tracking
This algorithm works like this:
```
  assignToCPU(workload):
    if (assignedAll workload):
      if (workload standard deviation is the smallest) return current solution
      return and keep trying

    for pCPU in pCPUList:
      assign workload to pCPU

  for workload in workloads:
    assignToCPU(workload)
```

This algorithm yields optimal solution as it tries all the possible solution. But due to timing, I didn't implement it.

### log files Explained (all with an interval of 2 seconds)
There are main principle in the scheduler design:
1. Avoid intervening if workload is distributed across pCPUs in balance.
2. Once imbalance is detected, the scheduler runs the rebalance algorithm.

To do this, the scheduler keeps profiling the workload across all pCPUs and maintains two metrics.
1. An average CPU time delta calculated from all pCPUs.
2. CPU time delta of each pCPUs.

The policy that determines if rebalance algorithm should run is based on the rule that if there is pCPU CPU time delta is different from the average CPU time delta by more than 10%, the scheduler thinks imbalance occurs in this case, and runs the rebalance algorithm.

#### log1 -- test1
The scheduler immediately rebalance the workload as shown in the file, then it just does nothing since it is balanced.

#### log2 -- test2
Same result as log1

#### log3 -- test3
It does nothing all the time since the workload are balanced

#### log4 -- test4
The scheduler does nothing for a long time since the workload are balanced most of the time. But when imbalance occurs, the algorithm detects it and rebalances workload as shown in the log file (please scroll down a bit.)

#### log5 -- test5
The scheduler also waits a bit for imbalance to occur. Then it rebalances the workload.
