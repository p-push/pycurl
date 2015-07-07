#! /usr/bin/env python
# -*- coding: utf-8 -*-
# vi:ts=4:et

import pycurl
import unittest

class ErrorCodesTest(unittest.TestCase):
    def test_error_codes(self):
        self.assertEqual('E_URL_MALFORMAT', pycurl.ERROR_CODES[3])
