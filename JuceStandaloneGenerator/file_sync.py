import os
import shutil
import hashlib
import stat
from typing import List, Tuple, Set
from enum import Enum

class ColorCode:
    """ANSI color codes for terminal output"""
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RESET = '\033[0m'

class SyncAction(Enum):
    COPIED = "Copied"
    UPDATED = "Updated"
    SKIPPED = "Skipped"
    ERROR = "Error"

class FileSync:
    def __init__(self, source_folder: str, target_folder: str, protected_files: Set[str] = None):
        self.source_folder = source_folder
        self.target_folder = target_folder
        self.protected_files = protected_files or set()

    @staticmethod
    def calculate_sha256(file_path: str) -> str:
        """Calculate the SHA256 hash of a file."""
        try:
            with open(file_path, 'rb') as f:
                return hashlib.sha256(f.read()).hexdigest()
        except (OSError, IOError) as e:
            raise RuntimeError(f"Failed to calculate hash for {file_path}: {e}")

    def _print_colored(self, action: SyncAction, message: str) -> None:
        """Print colored message based on action type."""
        color_map = {
            SyncAction.COPIED: ColorCode.GREEN,
            SyncAction.UPDATED: ColorCode.GREEN,
            SyncAction.SKIPPED: ColorCode.YELLOW,
            SyncAction.ERROR: ColorCode.RED
        }
        color = color_map.get(action, ColorCode.RESET)
        print(f"{color}{action.value}: {message}{ColorCode.RESET}")

    def _is_protected_file(self, relative_path: str) -> bool:
        """Check if a file is in the protected files set."""
        normalized_path = relative_path.replace(os.sep, '/')
        return any(
            normalized_path == protected_file.replace(os.sep, '/') or
            normalized_path.endswith('/' + protected_file.replace(os.sep, '/'))
            for protected_file in self.protected_files
        )

    def copy_changed_files(self) -> List[Tuple[SyncAction, str]]:
        """Copy files from source to target folder if content has changed."""
        results = []

        for root, dirs, files in os.walk(self.source_folder):
            rel_path = os.path.relpath(root, self.source_folder)
            target_dir = os.path.join(self.target_folder, rel_path)

            try:
                if not os.path.exists(target_dir):
                    os.makedirs(target_dir)
            except OSError as e:
                error_msg = f"Failed to create directory {target_dir}: {e}"
                results.append((SyncAction.ERROR, error_msg))
                continue

            for file in files:
                source_file = os.path.join(root, file)
                target_file = os.path.join(target_dir, file)
                relative_file_path = os.path.relpath(target_file, self.target_folder)

                try:
                    if (self._is_protected_file(relative_file_path) and
                            os.path.exists(target_file)):
                        results.append((SyncAction.SKIPPED,
                                        f"{relative_file_path} (protected file already exists)"))
                        continue

                    if os.path.exists(target_file):
                        # Make the target file writable before updating
                        try:
                            os.chmod(target_file, stat.S_IWRITE | stat.S_IREAD)
                        except OSError as e:
                            results.append((SyncAction.ERROR,
                                            f"Failed to make {relative_file_path} writable: {e}"))
                            continue

                        # Compare file hashes
                        try:
                            source_hash = self.calculate_sha256(source_file)
                            target_hash = self.calculate_sha256(target_file)

                            if source_hash != target_hash:
                                shutil.copy2(source_file, target_file)
                                results.append((SyncAction.UPDATED, relative_file_path))


                        except RuntimeError as e:
                            results.append((SyncAction.ERROR, str(e)))
                            continue
                    else:
                        # File doesn't exist in target, copy it
                        shutil.copy2(source_file, target_file)
                        results.append((SyncAction.COPIED, relative_file_path))

                    # Make the target file readonly after copying or updating
                    # if os.path.exists(target_file):
                    #     try:
                    #         os.chmod(target_file, stat.S_IREAD)
                    #     except OSError as e:
                    #         results.append((SyncAction.ERROR,
                    #                         f"Failed to make {relative_file_path} readonly: {e}"))

                except (OSError, IOError, shutil.Error) as e:
                    results.append((SyncAction.ERROR,
                                    f"Failed to sync {relative_file_path}: {e}"))

        return results

    def sync(self) -> None:
        results = self.copy_changed_files()

        if results:
            counts = {action: 0 for action in SyncAction}

            for action, message in results:
                self._print_colored(action, message)
                counts[action] += 1

            print(f"\n{ColorCode.GREEN}Sync completed:{ColorCode.RESET}")
            if counts[SyncAction.COPIED] > 0:
                print(f"  {counts[SyncAction.COPIED]} files copied")
            if counts[SyncAction.UPDATED] > 0:
                print(f"  {counts[SyncAction.UPDATED]} files updated")
            if counts[SyncAction.SKIPPED] > 0:
                print(f"  {ColorCode.YELLOW}{counts[SyncAction.SKIPPED]} files skipped (protected){ColorCode.RESET}")
            if counts[SyncAction.ERROR] > 0:
                print(f"  {ColorCode.RED}{counts[SyncAction.ERROR]} errors occurred{ColorCode.RESET}")
        else:
            print(f"{ColorCode.GREEN}No files needed updating.{ColorCode.RESET}")
