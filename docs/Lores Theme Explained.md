# Lore Color System

## Overview
Lore uses the **Okabe-Ito palette**, designed for universal accessibility across all color vision types. Blue + Orange remains distinct for red-green colorblind users (6-8% of population).

## Token Structure
Every color comes from a token—no hardcoded hex values below the token block.

### Base Surfaces
- `@base` — App background
    - Lore: `#0D1929` | Normal: `#0A0F1F` | Overseer: `#050A15`
- `@surface0` — Panels, cards
    - Lore: `#131D35` | Normal: `#131D35` | Overseer: `#0D1420`
- `@surface1` — Raised controls, toolbar buttons
    - Lore: `#1A2A48` | Normal: `#1A2A48` | Overseer: `#141F30`
- `@surface2` — Pressed/selected controls
    - Lore: `#24365C` | Normal: `#24365C` | Overseer: `#1D2A40`
- `@surface-raised` — Menus, popovers, tooltips
    - Lore: `#131D35` | Normal: `#131D35` | Overseer: `#0D1420`
- `@structure` — Deepest shadows
    - Lore: `#1A2A48` | Normal: `#1A2A48` | Overseer: `#141F30`

### Text
- `@text` — Primary readable text (4.5:1 contrast minimum)
    - Lore: `#E8F0F8` | Normal: `#E0E8F0` | Overseer: `#D8E0E8`
- `@text-muted` — Secondary text, hints
    - Lore: `#A8BED0` | Normal: `#9EAFC0` | Overseer: `#8A9DB0`
- `@text-subtle` — Placeholders, disabled labels
    - Lore: `#7A96B4` | Normal: `#7A8FA8` | Overseer: `#6A7F98`
- `@text-disabled` — Uninteractable elements
    - Lore: `#5A7A98` | Normal: `#5A7090` | Overseer: `#4A6080`

### Accent (Orange)
- `@accent` — Main interactive color
    - All: `#E69F00`
- `@accent-hover` — Hover state
    - All: `#F0AE1A`
- `@accent-pressed` — Pressed state
    - All: `#D89000`
- `@accent-muted` — Low-emphasis (progress bars, indicators)
    - All: `#F5C44D`
- `@accent-fg` — Text on accent
    - Lore: `#0D1929` | Normal: `#0A0F1F` | Overseer: `#050A15`

### Semantic Status
- `@success` — Completed action
    - All: `#009E73` (Bluish Green)
- `@warning` — User should notice
    - All: `#F0E442` (Yellow)
- `@error` — Failure/alert
    - All: `#D55E00` (Vermillion)
- `@info` — Neutral information
    - All: `#56B4E9` (Sky Blue)

### Hints & Borders
- `@hint-cool` — Informational highlights
    - All: `#56B4E9` (Sky Blue)
- `@hint-warm` — Warm highlights
    - All: `#D55E00` (Vermillion)
- `@hint-neutral` — Neutral highlights
    - All: `#009E73` (Bluish Green)
- `@border` — Default dividers, outlines
    - Lore: `#2A4A75` | Normal: `#24365C` | Overseer: `#1D2A40`
- `@border-strong` — Emphasized outlines
    - All: `#E69F00` (Orange Accent)
- `@divider` — Thin row separators
    - Lore: `#1F3A5F` | Normal: `#1A2A48` | Overseer: `#141F30`

## Core Rules

1. **All tokens must be defined** — No fallbacks
2. **Never use color alone** — Pair with icons, text, or patterns
3. **Contrast ≥ 4.5:1** for body text (WCAG AA)
4. **Semantic colors are trusted** — Don't use `@error` for decoration
5. **Both modes share vocabulary** — Differentiate with layout, not layout

## Why These Colors

- **Deep Navy Base** (`#0D1929`–`#050A15`): Confident, professional, reduces eye strain
- **Orange Accent** (`#E69F00`): Universally accessible across all colorblindness types
- **Blue/Green/Yellow Semantics**: Okabe-Ito research palette—proven accessible
- **No Red/Green Reliance**: Avoids 6-8% of male population completely

## Typography
- **UI:** Inter, Segoe UI, Helvetica Neue
- **Code:** JetBrains Mono, Fira Code, Courier New
- **Size:** 13pt base, 11pt monospace, 12pt buttons/menus

## Testing
- Run palettes through [Colorblind Simulator](https://www.crazy-colors.net/en/colorblind/)
- Check contrast with [WebAIM](https://webaim.org/resources/contrastchecker/)
- Verify 4.5:1 minimum on critical text

## When Adding Colors
Ask: *Does this need a new token, or does an existing one work?* The theme is a coat, not a skeleton.