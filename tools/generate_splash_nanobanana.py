#!/usr/bin/env python3
"""
Automate Nano Banana AI to generate a luxury splash video for HeadUnit.
Uses Playwright to navigate the website, enter a prompt, and download the result.
"""

import sys
import os
import time
from playwright.sync_api import sync_playwright

OUTPUT_DIR = "/home/mike/HeadUnit/themes/Monochrome"
DOWNLOAD_PATH = os.path.join(OUTPUT_DIR, "splash_nanobanana.mp4")

# Cinematic prompt for luxury monochrome Chevrolet splash
PROMPT = (
    "Luxury car brand boot screen animation on pure black background. "
    "A minimalist white Chevrolet bowtie logo slowly fades in from darkness "
    "with a subtle scale settling effect, appearing crisp and premium. "
    "After a pause, thin elegant white text 'CHEVROLET' fades in below "
    "with wide letter spacing, followed by smaller text 'BY GENERAL MOTORS' "
    "in a lighter weight. A subtle thin horizontal line separates the logo "
    "from the text. Very subtle film grain texture throughout. "
    "Gentle vignette darkening at edges. Everything holds for a moment "
    "then gracefully fades to black. Monochrome, no color, ultra premium, "
    "Bentley/Rolls-Royce quality aesthetic. 1080p cinematic."
)


def main():
    print(f"Prompt: {PROMPT[:80]}...")
    print(f"Output: {DOWNLOAD_PATH}")
    print()

    with sync_playwright() as p:
        # Launch with a visible browser on the Jetson display
        # Use headless=False so user can see what's happening
        browser = p.chromium.launch(
            headless=False,
            args=[
                '--no-sandbox',
                '--disable-gpu',
                '--window-size=1200,800',
            ]
        )

        context = browser.new_context(
            viewport={'width': 1200, 'height': 800},
            accept_downloads=True,
        )
        page = context.new_page()

        print("1. Navigating to nanobanana.io...")
        page.goto("https://nanobanana.io/text-to-video", timeout=30000)
        page.wait_for_load_state("networkidle", timeout=15000)

        # Take a screenshot to see the page state
        page.screenshot(path="/tmp/nanobanana_01_loaded.png")
        print("   Screenshot: /tmp/nanobanana_01_loaded.png")

        # Look for the text input area
        print("2. Looking for prompt input...")

        # Try common selectors for text areas
        textarea = None
        selectors = [
            'textarea',
            'textarea[placeholder*="prompt"]',
            'textarea[placeholder*="describe"]',
            'textarea[placeholder*="Enter"]',
            '[contenteditable="true"]',
            'input[type="text"]',
            '.prompt-input textarea',
            '#prompt',
        ]

        for sel in selectors:
            try:
                el = page.query_selector(sel)
                if el and el.is_visible():
                    textarea = el
                    print(f"   Found input: {sel}")
                    break
            except:
                continue

        if not textarea:
            print("   Could not find text input, taking debug screenshot...")
            page.screenshot(path="/tmp/nanobanana_02_no_input.png")
            # Print page content for debugging
            print("   Page title:", page.title())
            # Try to find any interactive elements
            inputs = page.query_selector_all("textarea, input, [contenteditable]")
            print(f"   Found {len(inputs)} input elements")
            for i, inp in enumerate(inputs):
                print(f"     [{i}] tag={inp.evaluate('el => el.tagName')}, "
                      f"visible={inp.is_visible()}, "
                      f"placeholder={inp.get_attribute('placeholder')}")
            browser.close()
            return

        # Type the prompt
        print("3. Entering prompt...")
        textarea.click()
        textarea.fill(PROMPT)
        time.sleep(1)

        page.screenshot(path="/tmp/nanobanana_03_prompt_entered.png")
        print("   Screenshot: /tmp/nanobanana_03_prompt_entered.png")

        # Look for settings to configure (resolution, duration, aspect ratio)
        print("4. Configuring video settings...")

        # Try to find and set aspect ratio to 16:9 if there's a selector
        aspect_selectors = [
            'text=16:9',
            'button:has-text("16:9")',
            '[data-value="16:9"]',
            'label:has-text("16:9")',
        ]
        for sel in aspect_selectors:
            try:
                el = page.query_selector(sel)
                if el and el.is_visible():
                    el.click()
                    print(f"   Set aspect ratio: {sel}")
                    break
            except:
                continue

        # Try to set duration to ~10 seconds
        duration_selectors = [
            'text=10s',
            'button:has-text("10")',
            'input[type="range"]',
        ]
        for sel in duration_selectors:
            try:
                el = page.query_selector(sel)
                if el and el.is_visible():
                    if el.evaluate('el => el.tagName') == 'INPUT':
                        el.fill('10')
                    else:
                        el.click()
                    print(f"   Set duration: {sel}")
                    break
            except:
                continue

        page.screenshot(path="/tmp/nanobanana_04_settings.png")

        # Find and click the generate button
        print("5. Looking for generate button...")
        generate_selectors = [
            'button:has-text("Generate")',
            'button:has-text("Create")',
            'button:has-text("Make")',
            'button[type="submit"]',
            '.generate-btn',
            '#generate',
            'button:has-text("Start")',
        ]

        generate_btn = None
        for sel in generate_selectors:
            try:
                el = page.query_selector(sel)
                if el and el.is_visible():
                    generate_btn = el
                    print(f"   Found button: {sel}")
                    break
            except:
                continue

        if not generate_btn:
            print("   Could not find generate button, taking debug screenshot...")
            page.screenshot(path="/tmp/nanobanana_05_no_button.png")
            buttons = page.query_selector_all("button")
            print(f"   Found {len(buttons)} buttons:")
            for i, btn in enumerate(buttons[:10]):
                print(f"     [{i}] text='{btn.inner_text()[:50]}', visible={btn.is_visible()}")
            browser.close()
            return

        print("6. Clicking generate...")
        generate_btn.click()

        page.screenshot(path="/tmp/nanobanana_06_generating.png")
        print("   Screenshot: /tmp/nanobanana_06_generating.png")

        # Wait for video generation (can take 1-3 minutes)
        print("7. Waiting for video generation (this may take a few minutes)...")
        max_wait = 300  # 5 minutes max
        start_time = time.time()
        video_url = None

        while time.time() - start_time < max_wait:
            time.sleep(10)
            elapsed = int(time.time() - start_time)
            print(f"   Waiting... {elapsed}s elapsed")

            # Take periodic screenshots
            if elapsed % 30 == 0:
                page.screenshot(path=f"/tmp/nanobanana_progress_{elapsed}s.png")

            # Check for video element or download link
            video_el = page.query_selector("video source, video[src], a[href*='.mp4'], a[href*='.webm'], a[download]")
            if video_el:
                src = video_el.get_attribute("src") or video_el.get_attribute("href")
                if src:
                    video_url = src
                    print(f"   Video found: {src[:80]}...")
                    break

            # Check for download button
            download_btn = page.query_selector("button:has-text('Download'), a:has-text('Download')")
            if download_btn and download_btn.is_visible():
                print("   Download button appeared!")
                page.screenshot(path="/tmp/nanobanana_07_ready.png")

                # Try to download
                with page.expect_download(timeout=60000) as download_info:
                    download_btn.click()
                download = download_info.value
                download.save_as(DOWNLOAD_PATH)
                print(f"   Downloaded to: {DOWNLOAD_PATH}")
                video_url = DOWNLOAD_PATH
                break

            # Check for error messages
            error_el = page.query_selector(".error, [class*='error'], [role='alert']")
            if error_el and error_el.is_visible():
                error_text = error_el.inner_text()
                if error_text:
                    print(f"   Error detected: {error_text[:100]}")

        if not video_url:
            print("   Timed out waiting for video. Taking final screenshot...")
            page.screenshot(path="/tmp/nanobanana_timeout.png")
        else:
            # If we got a URL but didn't download yet, download it
            if video_url.startswith("http"):
                print(f"8. Downloading video from: {video_url[:80]}...")
                import urllib.request
                urllib.request.urlretrieve(video_url, DOWNLOAD_PATH)
                print(f"   Saved to: {DOWNLOAD_PATH}")

        page.screenshot(path="/tmp/nanobanana_final.png")
        print("   Final screenshot: /tmp/nanobanana_final.png")

        browser.close()

    if os.path.exists(DOWNLOAD_PATH):
        size = os.path.getsize(DOWNLOAD_PATH)
        print(f"\nSuccess! Video: {DOWNLOAD_PATH} ({size / 1024 / 1024:.1f} MB)")
    else:
        print(f"\nNo video downloaded. Check screenshots in /tmp/nanobanana_*.png")


if __name__ == "__main__":
    main()
