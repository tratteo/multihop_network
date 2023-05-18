#!/usr/bin/env python3

from __future__ import division

import re
import sys
import os.path
import argparse
import numpy as np
import pandas as pd
from datetime import datetime

sink_id = 1 # Change this value if you decide to consider another sink!

# Firefly addresses
addr_id_map = {
    "f7:9c":  1, "d9:76":  2, "f3:84":  3, "f3:ee":  4, "f7:92":  5,
    "f3:9a":  6, "de:21":  7, "f2:a1":  8, "d8:b5":  9, "f2:1e": 10,
    "d9:5f": 11, "f2:33": 12, "de:0c": 13, "f2:0e": 14, "d9:49": 15,
    "f3:dc": 16, "d9:23": 17, "f3:8b": 18, "f3:c2": 19, "f3:b7": 20,
    "de:e4": 21, "f3:88": 22, "f7:9a": 23, "f7:e7": 24, "f2:85": 25,
    "f2:27": 26, "f2:64": 27, "f3:d3": 28, "f3:8d": 29, "f7:e1": 30,
    "de:af": 31, "f2:91": 32, "f2:d7": 33, "f3:a3": 34, "f2:d9": 35,
    "d9:9f": 36, "f3:90": 50, "f2:3d": 51, "f7:ab": 52, "f7:c9": 53,
    "f2:6c": 54, "f2:fc": 56, "f1:f6": 57, "f3:cf": 62, "f3:c3": 63,
    "f7:d6": 64, "f7:b6": 65, "f7:b7": 70, "f3:f3": 71, "f1:f3": 72,
    "f2:48": 73, "f3:db": 74, "f3:fa": 75, "f3:83": 76, "f2:b4": 77
}

nodes = []

def parse_file(log_file, testbed=False):
    # Print some basic information for the user
    print(f"Logfile: {log_file}")
    print(f"{'Cooja simulation' if not testbed else 'Testbed experiment'}\n")

    # Create CSV output files
    fpath = os.path.dirname(log_file)
    fname_common = os.path.splitext(os.path.basename(log_file))[0]
    frecv_name = os.path.join(fpath, f"{fname_common}-recv.csv")
    fsent_name = os.path.join(fpath, f"{fname_common}-sent.csv")
    fsrrecv_name = os.path.join(fpath, f"{fname_common}-srecv.csv")
    fsrsent_name = os.path.join(fpath, f"{fname_common}-ssent.csv")
    fenergest_name = os.path.join(fpath, f"{fname_common}-energest.csv")
    
    # Open CSV output files
    frecv = open(frecv_name, 'w')
    fsent = open(fsent_name, 'w')
    fsrrecv = open(fsrrecv_name, 'w')
    fsrsent = open(fsrsent_name , 'w')
    fenergest = open(fenergest_name, 'w')

    # Write CSV headers
    frecv.write("time\tdest\tsrc\tseqn\thops\n")
    fsent.write("time\tdest\tsrc\tseqn\n")
    fsrrecv.write("time\tdest\tsrc\tseqn\thops\tmetric\n")
    fsrsent.write("time\tdest\tsrc\tseqn\n")
    fenergest.write("time\tnode\tcnt\tcpu\tlpm\ttx\trx\n")

    # Regular expressions
    if testbed:
        # Regex for testbed experiments
        testbed_record_pattern = r"\[(?P<time>.{23})\] INFO:firefly\.(?P<self_id>\d+): \d+\.firefly < b"
        regex_node = re.compile(r"{}'Rime started with address (?P<src1>\d+).(?P<src2>\d+)'".format(testbed_record_pattern))
        regex_recv = re.compile(r"{}'App: recv from (?P<src1>\w+):(?P<src2>\w+) seqn (?P<seqn>\d+) hops (?P<hops>\d+)'".format(testbed_record_pattern))
        regex_sent = re.compile(r"{}'App: send seqn (?P<seqn>\d+)'".format(testbed_record_pattern))
        regex_srrecv = re.compile(r"{}'App: sr_recv from sink seqn (?P<seqn>\d+) hops (?P<hops>\d+) node metric (?P<metric>\d+)'".format(testbed_record_pattern))
        regex_srsent = re.compile(r"{}'App: sink sending seqn (?P<seqn>\d+) to (?P<dest1>\w+):(?P<dest2>\w+)'".format(testbed_record_pattern))
        regex_piggyback = re.compile(r"{}Protocol: piggyback topology update".format(record_pattern))
        regex_dedicated_topology =re.compile(r"{}Protocol: dedicated topology update".format(record_pattern))
        regex_dc = re.compile(r"{}'Energest: (?P<cnt>\d+) (?P<cpu>\d+) "
                              r"(?P<lpm>\d+) (?P<tx>\d+) (?P<rx>\d+)'".format(testbed_record_pattern))
    else:
        # Regular expressions for COOJA
        record_pattern = r"(?P<time>[\w:.]+)\s+ID:(?P<self_id>\d+)\s+"
        regex_node = re.compile(r"{}Rime started with address (?P<src1>\d+).(?P<src2>\d+)".format(record_pattern))
        regex_recv = re.compile(r"{}App: recv from (?P<src1>\w+):(?P<src2>\w+) seqn (?P<seqn>\d+) hops (?P<hops>\d+)".format(record_pattern))
        regex_sent = re.compile(r"{}App: send seqn (?P<seqn>\d+)".format(record_pattern))
        regex_srrecv = re.compile(r"{}App: sr_recv from sink seqn (?P<seqn>\d+) hops (?P<hops>\d+) node metric (?P<metric>\d+)".format(record_pattern))
        regex_srsent = re.compile(r"{}App: sink sending seqn (?P<seqn>\d+) to (?P<dest1>\w+):(?P<dest2>\w+)".format(record_pattern))
        regex_piggyback = re.compile(r"{}Protocol: piggyback topology update".format(record_pattern))
        regex_dedicated_topology =re.compile(r"{}Protocol: dedicated topology update".format(record_pattern))
        regex_dc = re.compile(r"{}Energest: (?P<cnt>\d+) (?P<cpu>\d+) "
                              r"(?P<lpm>\d+) (?P<tx>\d+) (?P<rx>\d+)".format(record_pattern))

    # Check if any node resets
    num_resets = 0
    num_piggbacks = 0
    num_dedicated_topology_updates = 0
    # Parse log file and add data to CSV files
    with open(log_file, 'r') as f:
        for line in f:
            m = regex_piggyback.match(line)
            if m:
                num_piggbacks += 1
            m = regex_dedicated_topology.match(line)
            if m:
                num_dedicated_topology_updates += 1

            # Node boot
            m = regex_node.match(line)
            if m:
                # Get dictionary with data
                d = m.groupdict()
                node_id = int(d["self_id"])
                # Save data in the nodes list
                if node_id not in nodes:
                    nodes.append(node_id)
                else:
                    num_resets += 1
                    print("WARNING: node {} reset during the simulation.".format(node_id))
                # Continue with the following line
                continue

            # Energest Duty Cycle
            m = regex_dc.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                else:
                    ts = d["time"]

                # Write to CSV file
                fenergest.write("{}\t{}\t{}\t{}\t{}\t{}\t{}\n".format(
                    ts, d['self_id'], d['cnt'], d['cpu'], d['lpm'], d['tx'], d['rx']))
                # Continue with the following line
                continue

            # RECV 
            m = regex_recv.match(line)
            if m:
                # Get dictionary with data
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                    src_addr = "{}:{}".format(d["src1"], d["src2"])
                    try:
                        src = addr_id_map[src_addr]
                    except KeyError as e:
                        print("KeyError Exception: key {} not found in "
                              "addr_id_map".format(src_addr))
                else:
                    ts = d["time"]
                    src = int(d["src1"], 16) # Discard second byte, and convert to decimal
                dest = int(d["self_id"])
                seqn = int(d["seqn"])
                hops = int(d["hops"])

                # Write to CSV file
                frecv.write("{}\t{}\t{}\t{}\t{}\n".format(ts, dest, src, seqn, hops))

                # Continue with the following line
                continue

            # SENT
            m = regex_sent.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                else:
                    ts = d["time"]
                src = int(d["self_id"])
                dest = sink_id
                seqn = int(d["seqn"])

                # Write to CSV file
                fsent.write("{}\t{}\t{}\t{}\n".format(ts, dest, src, seqn))

                # Continue with the following line
                continue

            # Source Routing RECV
            m = regex_srrecv.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                else:
                    ts = d["time"]
                src = sink_id
                dest = int(d["self_id"])
                seqn = int(d["seqn"])
                hops = int(d["hops"])
                metric = int(d["metric"])

                fsrrecv.write("{}\t{}\t{}\t{}\t{}\t{}\n".format(ts, dest, src, seqn, hops, metric))

                # Continue with the following line
                continue

            # Source Routing SENT
            m = regex_srsent.match(line)
            if m:
                d = m.groupdict()
                if testbed:
                    ts = datetime.strptime(d["time"], '%Y-%m-%d %H:%M:%S,%f')
                    ts = ts.timestamp()
                    dest_addr = "{}:{}".format(d["dest1"], d["dest2"])
                    try:
                        dest = addr_id_map[dest_addr]
                    except KeyError as e:
                        print("KeyError Exception: key {} not found in "
                              "addr_id_map".format(dest_addr))
                else:
                    ts = d["time"]
                    dest = int(d["dest1"], 16) # Discard second byte, and convert to decimal

                src = int(d["self_id"])
                seqn = int(d["seqn"])

                fsrsent.write("{}\t{}\t{}\t{}\n".format(ts, dest, src, seqn))

    # Close files
    frecv.close()
    fsent.close()
    fsrrecv.close()
    fsrsent.close()
    fenergest.close()

    if num_resets > 0:
        print("----- WARNING -----")
        print("{} nodes reset during the simulation".format(num_resets))
        print("") # To separate clearly from the following set of prints

    # Compute data collection statistics
    compute_collection_stats(fsent_name, frecv_name)

    # Compute source routing statistics
    compute_srouting_stats(fsrsent_name , fsrrecv_name)

    # Compute node duty cycle
    compute_node_duty_cycle(fenergest_name)

    compute_topology_updates_stats(num_piggbacks, num_dedicated_topology_updates)

def compute_topology_updates_stats(num_piggybacks, num_dedicated):
    total_updates = num_piggybacks + num_dedicated
    print("----- Topology updates -----")
    print("Piggybacks updates: {} > {:.2f}%".format(num_piggybacks, 100 * num_piggybacks / total_updates))
    print("Dedicated updates: {} > {:.2f}%".format(num_dedicated, 100 * num_dedicated / total_updates))


def compute_collection_stats(fsent_name, frecv_name):

    df_sent = pd.read_csv(fsent_name, sep='\t')
    df_recv = pd.read_csv(frecv_name, sep='\t')

    # Filter messages not received by the sink
    df_recv = df_recv[df_recv.dest == sink_id]

    # Remove duplicates, if any
    df_sent.drop_duplicates(['src', 'dest', 'seqn'], keep='first', inplace=True)
    df_recv.drop_duplicates(['src', 'dest', 'seqn'], keep='first', inplace=True)

    # Check if any node did not manage to send data
    fails = []
    for node_id in sorted(nodes):
        if node_id == sink_id:
            continue
        if node_id not in df_sent.src.unique():
            fails.append(node_id)
    if fails:
        print("----- Data Collection WARNING -----")
        for node_id in fails:
            print("Warning: node {} did not send any data.".format(node_id))
        print("") # To separate clearly from the following set of prints

    # Print node stats
    print("----- Data Collection Node Statistics -----\n")
    # Overall number of packets sent / received
    tsent = 0
    trecv = 0

    for node in sorted(df_sent.src.unique()):
        nsent = len(df_sent[df_sent.src == int(node)])
        nrecv = 0
        if node in (df_recv.src.unique()):
            nrecv = len(df_recv[df_recv.src == int(node)])

        pdr = 100 * nrecv / nsent
        print("Node {}: TX Packets = {}, RX Packets = {}, PDR = {:.2f}%, PLR = {:.2f}%".format(
            node, nsent, nrecv, pdr, 100 - pdr))

        # Update overall packets sent / received
        tsent += nsent
        trecv += nrecv

    # Print overall stats
    if tsent > 0:
        print("\n----- Data Collection Overall Statistics -----\n")
        print("Total Number of Packets Sent: {}".format(tsent))
        print("Total Number of Packets Received: {}".format(trecv))
        opdr = 100 * trecv / tsent
        print("Overall PDR = {:.2f}%".format(opdr))
        print("Overall PLR = {:.2f}%".format(100 - opdr))


def compute_srouting_stats(fsrsent_name , fsrrecv_name):

    df_srsent = pd.read_csv(fsrsent_name , sep='\t')
    df_srrecv = pd.read_csv(fsrrecv_name, sep='\t')

    # Filter messages not sent by the sink
    df_srsent = df_srsent[df_srsent.src == sink_id]

    # Remove duplicates, if any
    df_srsent.drop_duplicates(['src', 'dest', 'seqn'], keep='first', inplace=True)
    df_srrecv.drop_duplicates(['src', 'dest', 'seqn'], keep='first', inplace=True)

    # Print node stats
    print("\n----- Source Routing Node Statistics -----\n")
    # Overall number of packets sent / received
    tsrsent = 0
    tsrrecv = 0

    for node in sorted(df_srsent.dest.unique()):
        nsent = len(df_srsent[df_srsent.dest == int(node)])
        nrecv = 0
        if node in (df_srrecv.dest.unique()):
            nrecv = len(df_srrecv[df_srrecv.dest == int(node)])

        pdr = 100 * nrecv / nsent
        print("Node {}: TX Packets = {}, RX Packets = {}, PDR = {:.2f}%, PLR = {:.2f}%".format(
            node, nsent, nrecv, pdr, 100 - pdr))

        # Update overall packets sent / received
        tsrsent += nsent
        tsrrecv += nrecv

    # Print overall stats
    if tsrsent > 0:
        print("\n----- Source Routing Overall Statistics -----\n")
        print("Total Number of Packets Sent: {}".format(tsrsent))
        print("Total Number of Packets Received: {}".format(tsrrecv))
        opdr = 100 * tsrrecv / tsrsent
        print("Overall PDR = {:.2f}%".format(opdr))
        print("Overall PLR = {:.2f}%".format(100 - opdr))


def compute_node_duty_cycle(fenergest_name):

    # Read CSV file with dataframe
    df = pd.read_csv(fenergest_name, sep='\t')

    # Discard first two Energest report
    df = df[df.cnt >= 2].copy()

    # Create new df to store the results
    resdf = pd.DataFrame(columns=['node', 'dc'])

    print("\n----- Duty Cycle Statistics -----\n")
    # Iterate over nodes computing duty cyle
    # print("\n----- Node Duty Cycle -----")
    nodes = sorted(df.node.unique())
    dc_lst = []
    for node in nodes:
        rdf = df[df.node == node].copy()
        total_time = np.sum(rdf.cpu + rdf.lpm)
        total_radio = np.sum(rdf.tx + rdf.rx)
        dc = 100 * total_radio / total_time
        print("Node {}:  Duty Cycle: {:.3f}%".format(node, dc))
        dc_lst.append(dc)
        # Store the results in the DF
        idf = len(resdf.index)
        resdf.loc[idf] = [node, dc]

    print("\n----- Duty Cycle Overall Statistics -----\n")
    print("Average Duty Cycle: {:.3f}%\nStandard Deviation: {:.3f}\n"
          "Minimum: {:.3f}%\nMaximum: {:.3f}%\n".format(np.mean(dc_lst),
                                                        np.std(dc_lst), np.amin(dc_lst),
                                                        np.amax(dc_lst)))

    # Save DC dataframe to a CSV file (just in case)
    fpath = os.path.dirname(fenergest_name)
    fname_common = os.path.splitext(os.path.basename(fenergest_name))[0]
    fname_common = fname_common.replace('-energest', '')
    fdc_name = os.path.join(fpath, "{}-dc.csv".format(fname_common))
    # print("Saving Duty Cycle CSV file in: {}".format(fdc_name))
    resdf.to_csv(fdc_name, sep=',', index=False,
                 float_format='%.3f', na_rep='nan')

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('logfile', action="store", type=str,
                        help="data collection logfile to be parsed and analyzed.")
    parser.add_argument('-t', '--testbed', action='store_true',
                        help="flag for testbed experiments")
    return parser.parse_args()


if __name__ == '__main__':

    args = parse_args()
    print(args)

    if not args.logfile:
        print("Log file needs to be specified as 1st positional argument.")
    if not os.path.exists(args.logfile):
        print("The logfile argument {} does not exist.".format(args.logfile))
        sys.exit(1)
    if not os.path.isfile(args.logfile):
        print("The logfile argument {} is not a file.".format(args.logfile))
        sys.exit(1)

    # Parse log file, create CSV files, and print some stats
    parse_file(args.logfile, testbed=args.testbed)

