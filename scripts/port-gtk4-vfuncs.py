#!/usr/bin/env python3
"""Rewrite GTK3 GtkWidgetClass vfunc assignments to Verne GTK4 shims."""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]

MAP = {
    "button_press_event": "verne_widget_class_set_button_press_event",
    "button_release_event": "verne_widget_class_set_button_release_event",
    "motion_notify_event": "verne_widget_class_set_motion_notify_event",
    "key_press_event": "verne_widget_class_set_key_press_event",
    "key_release_event": "verne_widget_class_set_key_release_event",
    "scroll_event": "verne_widget_class_set_scroll_event",
    "enter_notify_event": "verne_widget_class_set_enter_notify_event",
    "leave_notify_event": "verne_widget_class_set_leave_notify_event",
    "focus_in_event": "verne_widget_class_set_focus_in_event",
    "focus_out_event": "verne_widget_class_set_focus_out_event",
    "draw": "verne_widget_class_set_draw",
    "delete_event": "verne_widget_class_set_delete_event",
    "popup_menu": "verne_widget_class_set_popup_menu",
    "style_updated": "verne_widget_class_set_style_updated",
    "grab_notify": "verne_widget_class_set_grab_notify",
    "configure_event": "verne_widget_class_set_configure_event",
    "get_accessible": "verne_widget_class_set_get_accessible",
}

ASSIGN = re.compile(
    r"(?P<indent>\s*)(?P<cls>(?:widget_class|klass|class|wclass|object_class))\s*->\s*(?P<field>"
    + "|".join(MAP.keys())
    + r")\s*=\s*(?P<val>[^;]+);"
)

# destroy and size_allocate / get_preferred_* / show are more specific
EXTRA = [
    (
        re.compile(r"(\s*)(\w+)\s*->\s*destroy\s*=\s*([^;]+);"),
        "destroy",
        "verne_widget_class_set_destroy",
        {"widget_class", "klass", "class", "wclass"},
    ),
    (
        re.compile(r"(\s*)(\w+)\s*->\s*size_allocate\s*=\s*([^;]+);"),
        "size_allocate",
        "verne_widget_class_set_size_allocate",
        {"widget_class", "klass", "class", "wclass"},
    ),
    (
        re.compile(r"(\s*)(\w+)\s*->\s*get_preferred_width\s*=\s*([^;]+);"),
        "get_preferred_width",
        "verne_widget_class_set_get_preferred_width",
        {"widget_class", "klass", "class", "wclass"},
    ),
    (
        re.compile(r"(\s*)(\w+)\s*->\s*get_preferred_height\s*=\s*([^;]+);"),
        "get_preferred_height",
        "verne_widget_class_set_get_preferred_height",
        {"widget_class", "klass", "class", "wclass"},
    ),
    (
        re.compile(r"(\s*)(\w+)\s*->\s*show\s*=\s*([^;]+);"),
        "show",
        "verne_widget_class_set_show",
        {"widget_class", "klass", "class", "wclass"},
    ),
]


def rewrite_text(text: str) -> str:
    def repl_assign(m):
        return f"{m.group('indent')}{MAP[m.group('field')]} ({m.group('cls')}, {m.group('val').strip()});"

    text = ASSIGN.sub(repl_assign, text)

    for rx, field, func, allowed in EXTRA:
        def extra_repl(m, func=func, allowed=allowed):
            cls = m.group(2)
            if cls not in allowed:
                return m.group(0)
            return f"{m.group(1)}{func} ({cls}, {m.group(3).strip()});"

        text = rx.sub(extra_repl, text)
    return text


def main():
    count = 0
    for path in ROOT.rglob("*.c"):
        if "libverne-compat" in str(path) or "/build" in str(path):
            continue
        original = path.read_text(errors="replace")
        updated = rewrite_text(original)
        if updated != original:
            path.write_text(updated)
            count += 1
            print(f"updated {path.relative_to(ROOT)}")
    print(f"rewrote {count} files")


if __name__ == "__main__":
    main()
