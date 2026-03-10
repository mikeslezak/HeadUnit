#!/usr/bin/env python3
"""
Render a cinematic splash video for HeadUnit themes.

Uses pycairo for frame rendering and GStreamer for MP4 encoding.
Produces a premium, cinematic boot animation with:
  - Smooth logo reveal with subtle scale
  - Elegant text fade with proper kerning
  - Film-grade vignette and grain
  - Precise easing curves

Usage:
  python3 render_splash.py [theme_dir] [output.mp4]
  python3 render_splash.py  # defaults to Monochrome theme
"""

import sys
import os
import json
import math
import struct
import subprocess
import tempfile
import numpy as np
import cairo
import gi
gi.require_version('Rsvg', '2.0')
from gi.repository import Rsvg

# ── Configuration ──
WIDTH = 1560
HEIGHT = 720
FPS = 30

# ── Easing functions ──

def ease_out_cubic(t):
    return 1.0 - (1.0 - t) ** 3

def ease_in_out_cubic(t):
    if t < 0.5:
        return 4.0 * t * t * t
    else:
        return 1.0 - (-2.0 * t + 2.0) ** 3 / 2.0

def ease_out_quart(t):
    return 1.0 - (1.0 - t) ** 4

def ease_in_out_quad(t):
    if t < 0.5:
        return 2.0 * t * t
    else:
        return 1.0 - (-2.0 * t + 2.0) ** 2 / 2.0

def ease_out_expo(t):
    if t >= 1.0:
        return 1.0
    return 1.0 - 2.0 ** (-10.0 * t)

def clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


class SplashRenderer:
    def __init__(self, theme_dir):
        self.theme_dir = theme_dir
        self.load_theme()
        self.load_assets()

    def load_theme(self):
        tokens_path = os.path.join(self.theme_dir, "tokens.json")
        with open(tokens_path) as f:
            self.tokens = json.load(f)

        splash = self.tokens.get("splash", {})
        palette = self.tokens.get("palette", {})
        typo = self.tokens.get("typography", {})

        # Colors
        self.bg_color = self.parse_color(splash.get("bg", palette.get("bg", "#000000")))
        self.text_color = self.parse_color(splash.get("color", "#C8C8C8"))
        self.line_color = self.parse_color(splash.get("lineColor", "#2A2A2A"))

        # Text
        self.primary_text = splash.get("primaryText", "CHEVROLET")
        self.secondary_text = splash.get("secondaryText", "BY GENERAL MOTORS")
        self.font_family = typo.get("fontFamily", "Inter Display")

        # Sizing
        self.logo_size = splash.get("logoSize", 300)
        self.title_size = splash.get("titleSize", 44)
        self.subtitle_size = splash.get("subtitleSize", 14)
        self.letter_spacing = splash.get("letterSpacing", 16)
        self.subtitle_letter_spacing = splash.get("subtitleLetterSpacing", 8)

        # Effects
        self.use_vignette = splash.get("useVignette", True)
        self.use_line = splash.get("useLine", True)

        # Font file
        self.font_file = os.path.join(self.theme_dir, "fonts",
                                       typo.get("fontFile", "fonts/InterDisplay-Regular.ttf").replace("fonts/", ""))

    def parse_color(self, hex_str):
        """Parse hex color to (r, g, b, a) floats 0-1."""
        hex_str = hex_str.lstrip('#')
        if len(hex_str) == 8:
            r, g, b, a = [int(hex_str[i:i+2], 16) / 255.0 for i in (0, 2, 4, 6)]
        elif len(hex_str) == 6:
            r, g, b = [int(hex_str[i:i+2], 16) / 255.0 for i in (0, 2, 4)]
            a = 1.0
        else:
            r, g, b, a = 0, 0, 0, 1
        return (r, g, b, a)

    def load_assets(self):
        """Load SVG logo."""
        logo_path = os.path.join(self.theme_dir, "logo.svg")
        self.logo_svg = Rsvg.Handle.new_from_file(logo_path)
        dim = self.logo_svg.get_dimensions()
        self.logo_aspect = dim.width / dim.height
        print(f"Logo: {dim.width}x{dim.height}, aspect={self.logo_aspect:.2f}")

    def draw_text_with_spacing(self, ctx, text, x, y, font_size, letter_spacing, weight=cairo.FONT_WEIGHT_BOLD):
        """Draw text with manual letter spacing, centered at x."""
        ctx.select_font_face(self.font_family, cairo.FONT_SLANT_NORMAL, weight)
        ctx.set_font_size(font_size)

        # Calculate total width with spacing
        total_width = 0
        char_widths = []
        for ch in text:
            extents = ctx.text_extents(ch)
            char_widths.append(extents.x_advance)
            total_width += extents.x_advance + letter_spacing
        total_width -= letter_spacing  # No spacing after last char

        # Draw centered
        cx = x - total_width / 2
        for i, ch in enumerate(text):
            ctx.move_to(cx, y)
            ctx.show_text(ch)
            cx += char_widths[i] + letter_spacing

        return total_width

    def draw_vignette(self, ctx):
        """Cinematic vignette — darkening at edges."""
        pat = cairo.RadialGradient(WIDTH / 2, HEIGHT / 2, HEIGHT * 0.25,
                                    WIDTH / 2, HEIGHT / 2, WIDTH * 0.7)
        pat.add_color_stop_rgba(0, 0, 0, 0, 0)
        pat.add_color_stop_rgba(0.6, 0, 0, 0, 0)
        pat.add_color_stop_rgba(1.0, 0, 0, 0, 0.75)
        ctx.set_source(pat)
        ctx.rectangle(0, 0, WIDTH, HEIGHT)
        ctx.fill()

    def add_film_grain(self, surface, intensity=0.015):
        """Add subtle film grain for organic feel."""
        data = surface.get_data()
        arr = np.frombuffer(data, dtype=np.uint8).reshape((HEIGHT, WIDTH, 4)).copy()

        # Generate grain — sparse, subtle
        grain = np.random.normal(0, intensity * 255, (HEIGHT, WIDTH)).astype(np.float32)

        for c in range(3):  # B, G, R (cairo is BGRA)
            channel = arr[:, :, c].astype(np.float32) + grain
            arr[:, :, c] = np.clip(channel, 0, 255).astype(np.uint8)

        # Write back
        result = cairo.ImageSurface.create_for_data(
            bytearray(arr.tobytes()), cairo.FORMAT_ARGB32, WIDTH, HEIGHT
        )
        return result

    def render_frame(self, t):
        """Render a single frame at time t (seconds).

        Timeline (seconds):
          0.0 - 0.5    Black
          0.5 - 2.5    Logo fades in (slow, from slight scale up)
          2.8 - 3.6    Line fades in
          3.0 - 4.5    Primary text fades in
          4.2 - 5.2    Secondary text fades in
          5.2 - 7.7    Hold (everything visible)
          7.7 - 9.7    Everything fades out together
          9.7 - 10.0   Black
        """
        surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, WIDTH, HEIGHT)
        ctx = cairo.Context(surface)

        # ── Background ──
        ctx.set_source_rgba(*self.bg_color)
        ctx.rectangle(0, 0, WIDTH, HEIGHT)
        ctx.fill()

        # ── Timeline calculations ──
        # Logo
        logo_start, logo_dur = 0.5, 2.0
        logo_t = clamp((t - logo_start) / logo_dur)
        logo_opacity = ease_out_quart(logo_t)
        logo_scale = 1.0 + 0.03 * (1.0 - ease_out_expo(logo_t))  # Starts 3% larger, settles

        # Line
        line_start, line_dur = 2.8, 0.8
        line_t = clamp((t - line_start) / line_dur)
        line_opacity = ease_out_cubic(line_t) * 0.5

        # Primary text
        text_start, text_dur = 3.0, 1.5
        text_t = clamp((t - text_start) / text_dur)
        text_opacity = ease_out_cubic(text_t)

        # Secondary text
        sub_start, sub_dur = 4.2, 1.0
        sub_t = clamp((t - sub_start) / sub_dur)
        sub_opacity = ease_out_cubic(sub_t)

        # Global fade out
        fade_start, fade_dur = 7.7, 2.0
        fade_t = clamp((t - fade_start) / fade_dur)
        global_opacity = 1.0 - ease_in_out_quad(fade_t)

        # ── Logo ──
        if logo_opacity > 0.001:
            ctx.save()
            logo_h = self.logo_size * 0.35
            logo_w = self.logo_size
            logo_x = (WIDTH - logo_w) / 2
            logo_y = HEIGHT * 0.28 - logo_h / 2

            # Scale from center
            cx = logo_x + logo_w / 2
            cy = logo_y + logo_h / 2
            ctx.translate(cx, cy)
            ctx.scale(logo_scale, logo_scale)
            ctx.translate(-cx, -cy)

            # Render SVG scaled to fit
            dim = self.logo_svg.get_dimensions()
            sx = logo_w / dim.width
            sy = logo_h / dim.height
            s = min(sx, sy)

            actual_w = dim.width * s
            actual_h = dim.height * s
            offset_x = logo_x + (logo_w - actual_w) / 2
            offset_y = logo_y + (logo_h - actual_h) / 2

            ctx.translate(offset_x, offset_y)
            ctx.scale(s, s)

            # Tint: render SVG then apply color
            ctx.push_group()
            self.logo_svg.render_cairo(ctx)
            logo_pattern = ctx.pop_group()

            # Apply opacity and color tint
            r, g, b, _ = self.text_color
            ctx.set_source_rgba(r, g, b, logo_opacity * global_opacity)
            ctx.mask(logo_pattern)

            ctx.restore()

        # ── Decorative line ──
        if self.use_line and line_opacity > 0.001:
            line_w = 120
            line_y = HEIGHT * 0.50
            r, g, b, _ = self.line_color
            ctx.set_source_rgba(r, g, b, line_opacity * global_opacity)
            ctx.rectangle((WIDTH - line_w) / 2, line_y, line_w, 1)
            ctx.fill()

        # ── Primary text ──
        if text_opacity > 0.001:
            r, g, b, _ = self.text_color
            ctx.set_source_rgba(r, g, b, text_opacity * global_opacity)
            self.draw_text_with_spacing(
                ctx, self.primary_text,
                WIDTH / 2, HEIGHT * 0.58,
                self.title_size, self.letter_spacing
            )

        # ── Secondary text ──
        if sub_opacity > 0.001:
            r, g, b, _ = self.text_color
            ctx.set_source_rgba(r, g, b, sub_opacity * global_opacity * 0.6)
            self.draw_text_with_spacing(
                ctx, self.secondary_text,
                WIDTH / 2, HEIGHT * 0.68,
                self.subtitle_size, self.subtitle_letter_spacing,
                weight=cairo.FONT_WEIGHT_NORMAL
            )

        # ── Vignette ──
        if self.use_vignette:
            self.draw_vignette(ctx)

        # ── Film grain ──
        if 0.3 < t < 9.5:
            surface = self.add_film_grain(surface, intensity=0.012)

        return surface

    def render_to_raw_frames(self, output_dir, duration=10.0):
        """Render all frames as raw BGRA files."""
        total_frames = int(duration * FPS)
        print(f"Rendering {total_frames} frames at {FPS}fps ({duration}s)...")

        for i in range(total_frames):
            t = i / FPS
            surface = self.render_frame(t)

            # Save as raw BGRA
            frame_path = os.path.join(output_dir, f"frame_{i:05d}.raw")
            data = surface.get_data()
            with open(frame_path, 'wb') as f:
                f.write(bytes(data))

            if i % FPS == 0:
                print(f"  {t:.1f}s / {duration:.1f}s")

        print("Frame rendering complete.")
        return total_frames

    def encode_mp4(self, frame_dir, output_path, total_frames, duration=10.0):
        """Encode raw frames to MP4 using GStreamer."""
        print(f"Encoding MP4: {output_path}")

        # Build GStreamer pipeline
        # Read raw BGRA frames, convert, encode with x264, mux to MP4
        pipeline_str = (
            f'multifilesrc location="{frame_dir}/frame_%05d.raw" '
            f'caps="video/x-raw,format=BGRx,width={WIDTH},height={HEIGHT},framerate={FPS}/1" '
            f'! videoconvert '
            f'! x264enc bitrate=8000 speed-preset=slow key-int-max={FPS * 2} '
            f'  option-string="keyint={FPS * 2}:min-keyint={FPS}:bframes=2" '
            f'! h264parse '
            f'! mp4mux '
            f'! filesink location="{output_path}"'
        )

        print(f"  Pipeline: gst-launch-1.0 {pipeline_str}")

        result = subprocess.run(
            ['gst-launch-1.0', '-e'] + pipeline_str.split(),
            capture_output=True, text=True, timeout=120
        )

        if result.returncode != 0:
            # GStreamer CLI parsing can be tricky with complex pipelines
            # Fall back to Python GStreamer bindings
            print("  gst-launch failed, using Python GStreamer API...")
            self.encode_mp4_python(frame_dir, output_path, total_frames)
        else:
            print(f"  Encoded: {output_path}")

    def encode_mp4_python(self, frame_dir, output_path, total_frames):
        """Encode using Python GStreamer API for reliable pipeline construction."""
        gi.require_version('Gst', '1.0')
        from gi.repository import Gst, GLib

        Gst.init(None)

        # Determine format from extension
        if output_path.endswith('.webm'):
            # VP8 + WebM — clean software decode path, universally compatible
            pipeline_str = (
                f'appsrc name=src '
                f'caps="video/x-raw,format=BGRx,width={WIDTH},height={HEIGHT},'
                f'framerate={FPS}/1" '
                f'! videoconvert '
                f'! vp8enc target-bitrate=8000000 cpu-used=4 '
                f'  end-usage=vbr undershoot=95 '
                f'  keyframe-max-dist={FPS * 2} '
                f'! webmmux '
                f'! filesink location="{output_path}"'
            )
        else:
            # H.264 baseline + MP4 — simple, compatible
            pipeline_str = (
                f'appsrc name=src '
                f'caps="video/x-raw,format=BGRx,width={WIDTH},height={HEIGHT},'
                f'framerate={FPS}/1" '
                f'! videoconvert '
                f'! x264enc bitrate=8000 speed-preset=medium bframes=0 '
                f'! video/x-h264,profile=baseline '
                f'! h264parse '
                f'! mp4mux '
                f'! filesink location="{output_path}"'
            )

        pipeline = Gst.parse_launch(pipeline_str)
        appsrc = pipeline.get_by_name("src")
        appsrc.set_property("format", Gst.Format.TIME)
        appsrc.set_property("is-live", False)

        pipeline.set_state(Gst.State.PLAYING)

        frame_size = WIDTH * HEIGHT * 4  # BGRA

        for i in range(total_frames):
            frame_path = os.path.join(frame_dir, f"frame_{i:05d}.raw")
            with open(frame_path, 'rb') as f:
                raw_data = f.read()

            buf = Gst.Buffer.new_allocate(None, frame_size, None)
            buf.fill(0, raw_data)

            # Set timestamps
            buf.pts = i * Gst.SECOND // FPS
            buf.duration = Gst.SECOND // FPS

            ret = appsrc.emit("push-buffer", buf)
            if ret != Gst.FlowReturn.OK:
                print(f"  Error pushing frame {i}: {ret}")
                break

            if i % FPS == 0:
                print(f"  Encoding: {i}/{total_frames}")

        appsrc.emit("end-of-stream")

        # Wait for EOS
        bus = pipeline.get_bus()
        bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.EOS | Gst.MessageType.ERROR)

        pipeline.set_state(Gst.State.NULL)
        print(f"  Encoded: {output_path}")


def main():
    theme_dir = sys.argv[1] if len(sys.argv) > 1 else "/home/mike/HeadUnit/themes/Monochrome"
    output_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(theme_dir, "splash.webm")

    print(f"Theme: {theme_dir}")
    print(f"Output: {output_path}")
    print(f"Resolution: {WIDTH}x{HEIGHT} @ {FPS}fps")

    renderer = SplashRenderer(theme_dir)

    with tempfile.TemporaryDirectory() as tmpdir:
        total_frames = renderer.render_to_raw_frames(tmpdir, duration=10.0)
        renderer.encode_mp4(tmpdir, output_path, total_frames)

    file_size = os.path.getsize(output_path)
    print(f"\nDone! {output_path} ({file_size / 1024 / 1024:.1f} MB)")


if __name__ == "__main__":
    main()
