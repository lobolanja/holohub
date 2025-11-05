"""Connext ANO transmit operator."""

from __future__ import annotations

import logging
from multiprocessing import shared_memory
from typing import Any, Optional

from connext_lib.endpoint_mng.user_data import DummyUserType
from holoscan.core import Operator, OperatorSpec


from connext_lib.cfg.common import ANOConfig, DDSConfig
from connext_lib.connext_writers import ConnextAnoWriter, ConnextDDSWriter




class ConnextAnoTxOp(Operator):
    """Minimal Connext transmit operator with ANO fallback placeholders."""

    def __init__(
        self,
        fragment,
        *args,
        dds_config: Optional[DDSConfig] = None,
        ano_config: Optional[ANOConfig] = None,
        **kwargs,
    ) -> None:
        super().__init__(fragment, *args, **kwargs)
        self._logger = logging.getLogger(f"{__name__}.{type(self).__name__}")
        self._dds_participant = None  # Placeholder for future DDS participant management

        self.connext_ano_writer = None # Placeholder for Connext ANO writer
        self.connext_dds_writer = None # Placeholder for Connext DDS writer

        self._dds_config = dds_config or DDSConfig()
        if self._dds_config.topic_class is None:
            self._dds_config.topic_class = DummyUserType

        self._ano_config = ano_config or ANOConfig()

        if self._ano_config.enabled:
            self._logger.info("Connext ANO TX enabled with transport '%s'", self._ano_config.transport)
            self.connext_ano_writer = ConnextAnoWriter(
                dds_config=self._dds_config,
                ano_config=self._ano_config
            )
        else:
            self._logger.info("Connext ANO TX disabled, operator will not transmit data")

        if self._dds_config.enabled:
            self._logger.info("DDS path enabled (domain=%s topic=%s)",
                                  self._dds_config.domain_id, self._dds_config.topic_name)
            self.connext_dds_writer = ConnextDDSWriter(dds_config=self._dds_config)
        else:
            self._logger.info("DDS path disabled")

    # ------------------------------------------------------------------
    def setup(self, spec: OperatorSpec) -> None:
        spec.input("input")

    def start(self) -> None:
        super().start()
        if self._ano_config.enabled:
            self.connext_ano_writer.start()


    def stop(self) -> None:
        if self._ano_config.enabled:
            self.connext_ano_writer.stop()
        super().stop()

    # ------------------------------------------------------------------
    def compute(self, op_input, _op_output, _context) -> None:
        payload = op_input.receive("input")
        if payload is None:
            return

        if self._ano_config.enabled:
            self._logger.info("Connext ANO TX write payload '%s'", payload)
            self.connext_ano_writer.write_buffer(payload)

        if self._dds_config.enabled:
            self._logger.info("DDS TX write payload '%s'", payload)
            message = self._dds_config.topic_class(data=payload)
            self.connext_dds_writer.write_message(message)
        else:
            self._logger.info("No DDS data source configured, skipping DDS write")
