"""Common types and helpers for Connext ANO Holoscan operators."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


# def __init__(self, fragment, dds_domain_id, dds_topic, dds_topic_class, *args, **kwargs):
@dataclass
class DDSConfig:
    """Configuration needed to interact with the DDS control plane."""

    domain_id: int = 0
    topic_name: str = "system_setup"
    topic_class: = None


@dataclass
class ANOConfig:
    """Settings for the ANO (RDMA/DPDK) fast path."""

    enabled: bool = False
    transport: str = "dpdk"  # TODO-JUANCA: detail supported transport strings
    device: Optional[str] = None
    # TODO-JUANCA: add queue/buffer pool settings once requirements are known


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
