"""Connext ANO receive operator."""

from __future__ import annotations

import logging
from multiprocessing import shared_memory
from typing import Optional

from connext_lib.connext_readers import ConnextAnoReader, ConnextDDSReader
from connext_lib.endpoint_mng.user_data import DummyUserType
from holoscan.core import Operator, OperatorSpec


from connext_lib.cfg.common import ANOConfig, DDSConfig



class ConnextAnoRxOp(Operator):
    """Minimal Connext receive operator with ANO placeholders."""

    def __init__(
        self,
        fragment,
        *args,
        dds_config: Optional[DDSConfig] = None,
        ano_config: Optional[ANOConfig] = None,
        **kwargs,
    ) -> None:
        super().__init__(fragment, *args, **kwargs)
        logging.basicConfig(level=logging.INFO)
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._dds_participant = None  # Placeholder for future DDS participant management
        self._dds_reader = None  # Placeholder for future DDS reader management
        self._connext_ano_reader = None # Placeholder for Connext ANO reader
        self._connext_dds_reader = None # Placeholder for Connext DDS reader

        self._dds_config = dds_config or DDSConfig()
        if self._dds_config.topic_class is None:
            self._dds_config.topic_class = DummyUserType

        self._ano_config = ano_config or ANOConfig()

        if self._ano_config.enabled:
            self._logger.info("Connext ANO enabled with transport '%s'", self._ano_config.transport)
            self._connext_ano_reader = ConnextAnoReader(self._dds_config, self._ano_config)
        else:
            self._logger.info("Connext ANO disabled, falling back to DDS only if enabled")

            if self._dds_config.enabled:
                self._logger.info("DDS path enabled (domain=%s topic=%s)",
                                  self._dds_config.domain_id, self._dds_config.topic_name)
                self._connext_dds_reader = ConnextDDSReader(dds_config=self._dds_config)

            else:
                self._logger.info("DDS path disabled. There are no data sources available.")

    # ------------------------------------------------------------------
    def setup(self, spec: OperatorSpec) -> None:
        spec.output("output")

    def start(self) -> None:
        super().start()


    def stop(self) -> None:
        # Receiver side currently has no background threads, but keep hook for symmetry
        super().stop()

    # ------------------------------------------------------------------
    def compute(self, _op_input, op_output, _context) -> None:
        # TODO: Review event base schedulers to use on_data_available
        # TODO: refactor this to its own class
        # TODO: two outputs, one for ANO and one for DDS
        if not self._ano_config.enabled:
            self._logger.info("Connext Ano Path not initialised")
            if self._dds_config.enabled:
                self._logger.info("DDS path enabled, reading from DDS")
                payload = self._connext_dds_reader.read_samples()
                op_output.emit(payload, "output")
            else:
                self._logger.warning("DDS path and ANO path disabled, no data source available")
        else:
            if self._connext_ano_reader.read_buffer():
                self._logger.info("Connext Ano path read")
                #TODO: (Irene) Still have to confirm if this is the correct way to get the data: shared memory ref or buffer?
                payload = self._connext_ano_reader.get_data()
                if payload:
                    op_output.emit(payload, "output")

