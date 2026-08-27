from __future__ import annotations

from rhythm_library.model import PatternSpec
from rhythm_library.patterns.cultural import CULTURAL_PATTERNS
from rhythm_library.patterns.educational import EDUCATIONAL_PATTERNS
from rhythm_library.patterns.looper import LOOPER_PATTERNS
from rhythm_library.patterns.performance import PERFORMANCE_PATTERNS


def all_patterns() -> list[PatternSpec]:
    return [*EDUCATIONAL_PATTERNS, *CULTURAL_PATTERNS, *LOOPER_PATTERNS, *PERFORMANCE_PATTERNS]
