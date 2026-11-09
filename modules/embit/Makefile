MODULE_NAME := embit
include ../python_module.mk

test:
	PYTHONPATH=../../include/bitcoinfuzz python3 -m unittest -v test_embit_lib

format: format-tests
format-tests:
	black ./test_embit_lib.py

check-format: check-format-tests
check-format-tests:
	black --check ./test_embit_lib.py

.PHONY: test format-tests check-format-tests
