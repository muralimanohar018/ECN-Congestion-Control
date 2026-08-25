# Intelligent ECN Congestion Controller

A modular ns-3 implementation of an ECN-driven congestion controller with a BDP-aware congestion-window constraint.

## Final architecture

- `traffic-generator` — installs TCP BulkSend and PacketSink applications.
- `ecn-monitor` — counts queue enqueues, ECN marks, ECN feedback events, and computes ECN ratio.
- `ecn-controller` — extends TCP NewReno and applies the proportional ECN-based CWND formula.
- `bdp-monitor` — measures RTT and computes BDP from bottleneck bandwidth and RTT.
- `bdp-controller` — constrains the ECN-derived CWND using the estimated BDP.
- `simulation/main.cc` — integrates the modules, builds the topology, installs RED+ECN, NetAnim, FlowMonitor, and outputs.

## Controller

ECN response:

`CWND_new = CWND_old * (1 - Alpha * ECN_Ratio)`

BDP:

`BDP = Bottleneck Bandwidth * RTT`

Final controller:

`Final CWND = min(Formula CWND, BDP)`

## Default experiment

- TCP: NewReno
- ECN: enabled
- Queue: RED + ECN
- Sender -> Router: 1 Gbps / 5 ms
- Router -> Receiver: 5 Mbps / 50 ms
- RED MinTh: 1 packet
- RED MaxTh: 3 packets
- Queue: 100 packets
- Packet count: 100000
- Packet size: 1000 bytes
- Simulation time: 30 seconds
- Alpha: 1.0

The RED thresholds are command-line parameters so they can be changed later without editing the source.

## Running inside ns-3.42

The modules can be compiled as part of the ns-3 scratch target. Copy or symlink this project's `simulation/main.cc` and `modules/` directory into the ns-3 source tree, or use the provided build helper.

Example:

```bash
./build-final.sh ~/ns-allinone-3.42/ns-3.42
```

Then:

```bash
cd ~/ns-allinone-3.42/ns-3.42
./ns3 run "scratch/ecn-final.cc --packetCount=100000 --simulationTime=30 --alpha=1.0"
```

Outputs are written in the ns-3 working directory:

- `ecn-final.xml`
- `ecn-final-results.csv`
- `ecn-final-flowmon.xml`

## Changing RED thresholds

For example:

```bash
./ns3 run "scratch/ecn-final.cc --minTh=20 --maxTh=50 --queuePackets=100"
```

The default values remain the verified reference configuration.
