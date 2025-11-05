"""Common types and helpers for Connext ANO Holoscan operators."""

from __future__ import annotations

from dataclasses import dataclass
from symtable import Class
from typing import Literal, Optional

SUPPORTED_TRANSPORTS: tuple[str, ...] = ("dpdk", "gpunetio")


@dataclass
class DDSConfig:
    """Configuration needed to interact with the DDS control plane."""
    enabled: bool = False
    domain_id: int = 0
    topic_name: str = "system_setup"
    topic_class: Class = None



@dataclass
class ANOConfig:
    """Settings for the ANO (RDMA/DPDK) fast path."""

    shm_name: str = "connext_ano_shm"
    shm_size: int = 1024
    event_name: str = "connext_tx_event"

    enabled: bool = False
    transport: Literal["dpdk", "gpunetio"] = "dpdk"
    device: Optional[str] = None
    # TODO-JUANCA: add queue/buffer pool settings once requirements are known

    def __post_init__(self) -> None:
        if self.transport not in SUPPORTED_TRANSPORTS:
            raise ValueError(
                f"Unsupported ANO transport '{self.transport}'. "
                f"Supported values: {', '.join(SUPPORTED_TRANSPORTS)}"
            )


class TransportState:
    """Lazily tracks whether ANO is active or we should fall back to DDS."""

    def __init__(self, ano_config: ANOConfig) -> None:
        self.ano_config = ano_config
        self.ano_active = False

    def activate_ano(self) -> None:
        # TODO-JUANCA: perform the real ANO bring-up handshake here
        if self.ano_config.enabled:
            self.ano_active = True

    def deactivate_ano(self) -> None:
        # TODO-JUANCA: tear down ANO resources if required
        self.ano_active = False

    def should_use_ano(self) -> bool:
        return self.ano_active and self.ano_config.enabled
