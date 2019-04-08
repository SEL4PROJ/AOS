#!/usr/bin/python3
#
# Copyright 2019, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(DATA61_GPL)
#
import argparse
import json
import numpy
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import sys
import scipy.stats as stats

KB = 1024.0
CCNT_SCALE = 64
CLOCK_HZ = (792000000)


def kb_per_s(cycles, size):
    return CLOCK_HZ * size / (CCNT_SCALE * cycles) / KB


def main():
    parser = argparse.ArgumentParser(description='Parse AOS filesystem benchmark results')
    parser.add_argument('--file', type=str, help='Results file', required='True', dest='file')
    args = parser.parse_args()

    xs = dict()
    ys = dict()
    errors = dict()
    all_samples = []

    with open(args.file, 'r') as json_in:
        try:
            rows = json.load(json_in)
        except ValueError as e:
            print("Json data invalid! error:\n {0}".format(e))
            return -1

    for row in rows:
        # append the buf size to the x axis
        if row['name'] not in xs:
            xs[row['name']] = []
            ys[row['name']] = []
            errors[row['name']] = []

        xs[row['name']].append(row.get('buf_size') / KB)

        # compute the checksum
        check_sum = sum(row['samples'])
        if check_sum != row['check_sum']:
            print('error! checksum incomplete (got {0} expected {1}) for row:\n {0}'.format(
                check_sum, row['check_sum'], row))
            return -1

        samples = [kb_per_s(sample, row['file_size']) for sample in row['samples']]
        all_samples = all_samples + samples
        # compute mean and stddev
        average = numpy.mean(samples)
        error = numpy.std(samples)

        ys[row['name']].append(average)
        errors[row['name']].append(error)

    figure, ax = plt.subplots(len(xs.keys()), sharex=True)

    # construct the graph
    for i, key in enumerate(xs):
        print(xs[key])
        ax[i].errorbar(xs[key], ys[key], yerr=errors[key], label=key, marker='o', linewidth=2)
        ax[i].grid(True)
        ax[i].set_ylabel('Speed (KB/s)')
        ax[i].set_title(key)
        ax[i].set_xscale('log', basex=2)
        ax[i].xaxis.set_ticks(xs[key])
        ax[i].xaxis.set_major_formatter(ticker.ScalarFormatter())

    ax[len(xs.keys())-1].set_xlabel('Buffer (KB)')
    plt.savefig('results.png')

    print("Harmonic mean of all samples: {} (KB/S)".format(str(stats.hmean(all_samples))))


if __name__ == '__main__':
    sys.exit(main())
