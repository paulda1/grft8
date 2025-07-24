#!/usr/bin/env python3

import sys
import unittest
from gnuradio import gr
from gnuradio import ft8

try:
    encoder = ft8.encoder("K1ABC W9XYZ 6A WI")
except Exception as e:
    print(e)
