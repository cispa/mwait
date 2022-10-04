#! /usr/bin/env python3

from itertools import groupby
import math

TRANSMITTED_PATTERN = "01011001"


def extend_to_manchester(pattern):
    res = ""
    for c in pattern:
        if c == "1":
            res += "01"
        elif c == "0":
            res += "10"
        else:
            assert False
    return res


def decode_manchester(manchester, offset=0):
    decoded = []

    valid = True
    for i in range(offset, len(manchester) - 1, 2):
        if manchester[i] == '0' and manchester[i + 1] == '1':
            decoded.append("1")
        elif manchester[i] == '1' and manchester[i + 1] == '0':
            decoded.append("0")
        else:
            decoded.append("X")
            valid = False
    return (valid, decoded)


def find_beginning(received_bits):
    target_pattern = extend_to_manchester(TRANSMITTED_PATTERN)
    for idx in range(len(received_bits)):
        if "".join(received_bits[idx:idx + len(target_pattern)]) == target_pattern:
            print(f"found beginning at idx {idx}")
            return idx
    print("Couldn't find beginning. Aborting")
    assert False

def find_decoded_ending(manchester_decoded_bits):
    # use this only AFTER manchester encoding was decoded
    for idx in range(len(manchester_decoded_bits)):
        idx_from_behind = len(manchester_decoded_bits) - 1 - idx
        c = manchester_decoded_bits[idx_from_behind]
        if c != "X":
            return idx_from_behind



def print_error_rate(received_bits_decoded):

    idx = 0
    correct = 0
    failed = 0
    consecutive_failures = 0
    sum_transmitted = 0
    for c in received_bits_decoded:
        if c == TRANSMITTED_PATTERN[idx]:
            correct += 1
            consecutive_failures = 0
        else:
            failed += 1
            consecutive_failures += 1
        idx = (idx + 1) % len(TRANSMITTED_PATTERN)
        sum_transmitted += 1

    consecutive_failures_threshold = 30
    if consecutive_failures > consecutive_failures_threshold:
        failed -= consecutive_failures_threshold
        sum_transmitted -= consecutive_failures_threshold


    correct_percentage = (correct / sum_transmitted) * 100

    print(f"Analyzed {len(received_bits_decoded)} decoded bits")
    print(f"Correct bits: {correct} ({correct_percentage}%)")
    print(f"Incorrect bits: {failed}")

def writeout_result(received_bits_decoded):
    with open("decoded.txt", "w") as fd:
        for idx in range(len(received_bits_decoded)-7):
            line = "".join(received_bits_decoded[idx:idx+8]) + "\n"
            fd.write(line)
def main():
    with open("recv.txt", "r") as fd_recv:
        received_bits = [i.strip() for i in fd_recv.readlines()]

        parts = ["".join(g) for k, g in groupby(received_bits)][2:]
        print(parts)
        one_parts = [len(x) for x in parts if x[0] == '1']
        zero_parts = [len(x) for x in parts if x[0] == '0']
        # we use idx 1 in the next two lines to filter out the lowest element
        # (sometimes measurement error)
        min_len1 = sorted(one_parts)[3]
        min_len0 = sorted(zero_parts)[3]
        manchester = []
        for p in parts:
            if p[0] == '0':
                frac = len(p) / min_len0
                c = int(round(frac, 0))
                print("0: %f (%d) - " % (frac, c))
                for i in range(c):
                    manchester.append("0")
            else:
                frac = len(p) / min_len1
                c = int(round(frac, 0))
                print("1: %f (%d) - " % (frac, c))
                for i in range(c):
                    manchester.append("1")
        # print(manchester)
        # exit()

        print("len(manchester):", len(manchester))
        start_offset = find_beginning(manchester)
        received_bits_decoded = decode_manchester(manchester, start_offset)[1]
        writeout_result(received_bits_decoded)
        print("len(received_bits_decoded):", len(received_bits_decoded))
        ending = find_decoded_ending(received_bits_decoded)
        print(f"Ending at {ending}/{len(received_bits_decoded)}")
        print_error_rate(received_bits_decoded[:ending+1])


if __name__ == "__main__":
    main()
