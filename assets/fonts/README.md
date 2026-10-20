# Font sources

- Cinzel Decorative Bold — Natanael Gama, SIL Open Font License 1.1. Source: https://github.com/google/fonts/tree/main/ofl/cinzeldecorative
- Alegreya Sans Medium — The Alegreya Sans Project Authors / Huerta Tipográfica, SIL Open Font License 1.1. Source: https://github.com/google/fonts/tree/main/ofl/alegreyasans

The corresponding complete licenses are in `licenses/`. Original TTF files are retained here. `tools/bake_fonts.py` bakes ASCII glyphs into the embedded alpha atlases in `src/font_data.hpp`; install `tools/requirements-fonts.txt` to regenerate them. The game does not require external font files or SDL_ttf at runtime.
