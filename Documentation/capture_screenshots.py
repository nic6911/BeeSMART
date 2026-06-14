#!/usr/bin/env python3
"""Generate language-specific screenshots from screenshot-template.html.

Usage:
    pip install playwright
    playwright install chromium
    python capture_screenshots.py

Produces: images/Billede4_DA.png, images/Billede4_EN.png, ... etc.
"""

import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
HTML_FILE = HERE / "screenshot-template.html"
OUTPUT_DIR = HERE / "images"

LANGUAGES = ["DA", "EN", "DE"]
SCREEN_IDS = ["Billede4", "Billede5", "Billede6", "Billede9"]


def capture():
    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("Error: playwright is not installed.", file=sys.stderr)
        print("Run:  pip install playwright && playwright install chromium", file=sys.stderr)
        sys.exit(1)

    if not HTML_FILE.exists():
        print(f"Error: {HTML_FILE} not found. Run this script from the Documentation/ folder.", file=sys.stderr)
        sys.exit(1)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    html_uri = HTML_FILE.resolve().as_uri()

    # Chromium needs certain system libraries that may not be installed.
    # If libnspr4.so is missing, point to local copies via LD_LIBRARY_PATH.
    browser_env = os.environ.copy()
    lib_path = "/tmp/chromium_libs/usr/lib/x86_64-linux-gnu"
    if os.path.isdir(lib_path):
        existing = browser_env.get("LD_LIBRARY_PATH", "")
        browser_env["LD_LIBRARY_PATH"] = f"{lib_path}:{existing}" if existing else lib_path

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True, env=browser_env)
        page = browser.new_page(viewport={"width": 1400, "height": 1200})

        page.goto(html_uri, wait_until="networkidle")

        for lang in LANGUAGES:
            # Switch language tab
            page.click(f"button[data-lang='{lang}']")
            page.wait_for_timeout(300)

            # Query only within the currently visible language section
            lang_section = page.query_selector(f"#lang-{lang}")

            for screen_id in SCREEN_IDS:
                selector = f"#mockup-{screen_id}-{lang}"
                element = lang_section.query_selector(selector) if lang_section else None
                if not element:
                    print(f"  WARNING: {selector} not found, skipping")
                    continue

                filename = f"{screen_id}_{lang}.png"
                filepath = OUTPUT_DIR / filename
                element.screenshot(path=str(filepath))
                print(f"  Saved: {filename}")

        browser.close()

    print(f"\nDone. {len(LANGUAGES) * len(SCREEN_IDS)} screenshots saved to {OUTPUT_DIR}/")


if __name__ == "__main__":
    capture()
