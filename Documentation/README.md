# Generating Language-Specific Screenshots

The LaTeX manuals reference language-specific screenshots for the 4 web UI tabs:

| Tab | Danish | English | German |
|---|---|---|---|
| Filling | `Billede4_DA.png` | `Billede4_EN.png` | `Billede4_DE.png` |
| Settings | `Billede5_DA.png` | `Billede5_EN.png` | `Billede5_DE.png` |
| Advanced | `Billede6_DA.png` | `Billede6_EN.png` | `Billede6_DE.png` |
| Statistics | `Billede9_DA.png` | `Billede9_EN.png` | `Billede9_DE.png` |

## Regenerating screenshots

When the web UI text changes, regenerate all 12 images at once:

```bash
# Install dependencies (one-time)
pip install playwright
playwright install chromium

# Install emoji font (required for statistics page icons 🍯🫙📏)
sudo apt install fonts-noto-color-emoji   # Linux
# or manually copy NotoColorEmoji.ttf to ~/.fonts/ and run fc-cache -f

# Capture screenshots
python3 capture_screenshots.py
```

This opens `screenshot-template.html` in a headless browser, switches through each language, and captures each mockup as a PNG in `images/`.

## How it works

- `screenshot-template.html` contains CSS mockups of all 4 tabs in all 3 languages
- `capture_screenshots.py` uses Playwright to take element-level screenshots of each mockup
- The mockups use the exact firmware strings from `Software/honeyDosing_v3/data/app.js`
