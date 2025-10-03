# conftest.py
import logging

def pytest_configure(config):
    logging.basicConfig(level=logging.INFO)  # or any desired log level
