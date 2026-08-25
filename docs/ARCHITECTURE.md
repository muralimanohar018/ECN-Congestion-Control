# Final Modular Architecture

```text
                         main.cc
                            |
                            v
                  +-------------------+
                  | Traffic Generator |
                  +---------+---------+
                            |
                            v
                         TCP
                            |
                            v
                       RED + ECN
                            |
              +-------------+-------------+
              |                           |
              v                           v
        ECN Monitor                 BDP Monitor
              |                           |
              v                           v
       ECN Controller                 RTT + BDP
              |                           |
              +-------------+-------------+
                            |
                            v
                     BDP Controller
                            |
                            v
                       Final CWND
                            |
                            v
                          TCP
```

## Separation of responsibilities

### Traffic Generator
Only creates the TCP source and sink.

### ECN Monitor
Only measures ECN-related events.

### ECN Controller
Owns the proposed proportional ECN response:

`CWND_new = CWND_old * (1 - Alpha * ECN_Ratio)`

### BDP Monitor
Measures RTT and computes:

`BDP = Bandwidth * RTT`

### BDP Controller
Applies:

`Final CWND = min(Formula CWND, BDP)`

### Main Simulation
Only integrates the modules and performs experiment setup/result collection.
