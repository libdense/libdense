PYTHON ?= python3

.PHONY: build clean test benchmark wheel

build:
	$(PYTHON) setup.py build_ext --inplace --force

test: build
	PYTHONPATH=src $(PYTHON) -m unittest discover -s tests -v

benchmark: build
	PYTHONPATH=src $(PYTHON) benchmarks/benchmark_binding.py

wheel:
	rm -rf wheelhouse
	$(PYTHON) -m pip wheel . --no-deps -w wheelhouse

clean:
	rm -rf build wheelhouse *.egg-info src/*.egg-info
	find src -name '_dense_sim*.so' -delete
	find . -type d -name '__pycache__' -prune -exec rm -rf {} +
