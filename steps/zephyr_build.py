from pathlib import Path
from typing import Any, Dict, List

from py_app_dev.core.exceptions import UserNotificationException
from pypeline.domain.execution_context import ExecutionContext
from pypeline.domain.pipeline import PipelineStep


class ZephyrBuild(PipelineStep[ExecutionContext]):
    """Builds one variant for one platform: `pypeline run -i platform=esp32h2 -i variant=disco`."""

    def run(self) -> None:
        platform_name = str(self.execution_context.get_input("platform"))
        variant = str(self.execution_context.get_input("variant"))
        platform = self.get_platform(platform_name)
        build_dir = self.project_root_dir / "build" / platform_name / variant

        command: List[str] = [
            "west",
            "build",
            "-b",
            platform["board"],
            "-d",
            build_dir.as_posix(),
            (self.project_root_dir / "app").as_posix(),
            "--",
            # The variant. Everything else the build needs is selected by the
            # board name: Zephyr looks for app/boards/<board>.overlay and
            # app/boards/<board>[_<variant>].conf on its own.
            f"-DFILE_SUFFIX={variant}",
            *self.get_toolchain_args(platform),
        ]
        self.execution_context.create_process_executor(command, cwd=self.project_root_dir).execute()

    def get_platform(self, name: str) -> Dict[str, Any]:
        platforms = (self.config or {}).get("platforms", {})
        if name not in platforms:
            raise UserNotificationException(f"Unknown platform '{name}'. Configured platforms: {', '.join(sorted(platforms))}.")
        return platforms[name]

    def get_toolchain_args(self, platform: Dict[str, Any]) -> List[str]:
        """Passed per build rather than exported: Zephyr caches them per build directory and gives environment variables the lowest precedence."""
        variant = platform["toolchain"]
        args = [f"-DZEPHYR_TOOLCHAIN_VARIANT={variant}"]
        if variant != "cross-compile":
            return args

        app = platform["toolchain_app"]
        # PoksInstall published every installed app's bin directory; the
        # toolchain root is one level up from it.
        bin_dirs = [d for d in self.execution_context.install_dirs if d.parent.parent.name == app]
        if not bin_dirs:
            raise UserNotificationException(f"Toolchain '{app}' is not installed. It must be listed in poks.json.")
        root = bin_dirs[0].parent

        args += [
            f"-DCROSS_COMPILE={(root / 'bin' / app).as_posix()}-",
            # Without this the toolchain's libc support is never probed, and
            # picolibc silently disappears from Kconfig.
            f"-DCROSS_COMPILE_TOOLCHAIN_PATH={root.as_posix()}",
        ]
        if "sysroot" in platform:
            args.append(f"-DSYSROOT_DIR={(root / platform['sysroot']).as_posix()}")
        return args

    def get_needs_dependency_management(self) -> bool:
        """Ninja already knows what is up to date; a second layer of caching on top would only get in the way."""
        return False

    def get_inputs(self) -> List[Path]:
        return []

    def get_outputs(self) -> List[Path]:
        return []

    def get_name(self) -> str:
        return self.__class__.__name__

    def update_execution_context(self) -> None:
        pass
