from pathlib import Path
from typing import List

from pypeline.domain.execution_context import ExecutionContext
from pypeline.domain.pipeline import PipelineStep


class ZephyrSetup(PipelineStep[ExecutionContext]):
    """Publishes the environment Zephyr needs, both to the steps that follow and to the generated env_setup scripts."""

    def run(self) -> None:
        return None

    def get_inputs(self) -> List[Path]:
        return []

    def get_outputs(self) -> List[Path]:
        return []

    def get_name(self) -> str:
        return self.__class__.__name__

    def update_execution_context(self) -> None:
        # Here rather than in run(): this method is called on every pipeline
        # invocation, including cache hits, and the variables must always be set.
        self.execution_context.add_env_vars(
            {
                # Zephyr's in-tree package search only looks for a directory named
                # `zephyr` among the application's ancestors, and this checkout is
                # at deps/zephyr (see west.yml for why it is not at the root).
                "ZEPHYR_BASE": (self.project_root_dir / "deps" / "zephyr").as_posix(),
                # native_sim runs on the host, so build with the host compiler
                # rather than looking for the Zephyr SDK.
                "ZEPHYR_TOOLCHAIN_VARIANT": "host",
            }
        )
