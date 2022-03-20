#!/bin/bash

set -e

jackd -P 40 -d dummy --rate ${JACK_SAMPLE_RATE} --period ${JACK_PERIOD}

