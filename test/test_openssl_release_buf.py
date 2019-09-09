#!/usr/bin/python3

import pytest

from common import *

filename = os.path.basename(__file__)

@pytest.fixture
def tc_setup():
    '''Initializes log file in TWT log module'''
    TWT_set_log_filename(filename)

@pytest.mark.parametrize("app1, app2, app1_args, app2_args, app1_res, app2_res", [
    #TODO Dont know how to verify whether actually release buffer is getting invoked
    # Enable Release buff mode on SSL Context
    ('test_openssl', 'test_openssl', ' -S -b 1 ', ' ', 0, 0),
    ('test_openssl', 'test_openssl', ' -S ', ' -b 1 ', 0, 0),

    # Enable Release buff mode on SSL
    ('test_openssl', 'test_openssl', ' -S -b 2 ', ' ', 0, 0),
    ('test_openssl', 'test_openssl', ' -S ', ' -b 1 ', 0, 0),

    # Enable Release buff mode on SSL
    ('test_openssl', 'test_openssl', ' -S -b 1 -V 13', ' -b 1 -V 13 ', 0, 0),
    ('test_openssl', 'test_openssl', ' -S -b 1 -V 12', ' -b 1 -V 12 ', 0, 0),
    ('test_openssl', 'test_openssl', ' -S -b 1 -V 11', ' -b 1 -V 11 ', 0, 0),
    ('test_openssl', 'test_openssl', ' -S -b 1 -V 10', ' -b 1 -V 10 ', 0, 0),
])

def test_openssl_tls_release_buf(tc_setup, app1, app2, app1_args, app2_args, app1_res, app2_res):
    assert run_serv_clnt_app([app1, app2, app1_args, app2_args, app1_res, app2_res]) == 0