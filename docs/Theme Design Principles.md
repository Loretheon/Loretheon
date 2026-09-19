# Lore — Theme Design Principles

## What themes are for

A theme changes how Lore looks without changing what it does. A user who
prefers dark should be able to work in dark without any feature going
missing; a user who wants a high-contrast theme should be able to get
one without the app looking like it was designed twice.

Themes are the user's, not the app's. The app ships with a default
light and a default dark; everything else is the community's.

## The token model

Every colour in the UI comes from a token. Tokens are defined in a
`@`-prefixed block at the top of a theme file and expanded into the
stylesheet at load time. No theme should hardcode a hex colour anywhere
below its token block.

The tokens are:

**Base surfaces**

- `base` — the app background
- `surface0` — panels and cards
- `surface1` — raised controls, toolbar buttons
- `surface2` — pressed or selected controls
- `surface-raised` — menus, popovers, tooltips
- `structure` — the deepest shadowed areas

**Text**

- `text` — primary readable text
- `text-muted` — secondary text, hints
- `text-subtle` — placeholder text, disabled labels
- `text-disabled` — anything the user cannot interact with

**Accent**

- `accent` — the main interactive colour (buttons, links, focus)
- `accent-hover` — accent in hover state
- `accent-pressed` — accent in pressed state
- `accent-muted` — a low-emphasis accent (progress bars, indicators)
- `accent-fg` — text that sits on top of `accent`

**Hints (non-semantic colours)**

- `hint-cool` — cool-toned highlights (informational, non-urgent)
- `hint-warm` — warm-toned highlights
- `hint-neutral` — neutral highlights

**Borders**

- `border` — the default divider and outline
- `border-strong` — emphasized outlines
- `divider` — thin separators between rows

**Status (semantic)**

- `success` — a completed action
- `warning` — something the user should notice
- `error` — something failed
- `info` — neutral, informational

## Rules a theme must follow

1. **Every token must be defined.** A theme that omits a token will
   fall back to the default; do not rely on that. Define them all.

2. **Contrast minimums.** Text on `base`, `surface0` and `surface1`
   must meet WCAG AA (4.5:1 for body, 3:1 for large text and UI
   elements). This is a hard requirement, not a suggestion.

3. **Status colours are semantic.** Do not use `success` as decoration.
   Do not use `error` for anything that is not a failure. Users learn
   to trust these colours; a theme that lies about them is broken.

4. **`accent` and `accent-fg` must be readable together.** If `accent`
   is dark, `accent-fg` should be light, and vice versa.

5. **Do not override the diagram colours directly.** Diagrams are
   themed by `SvgThemer`, which maps their internal colours onto the
   current theme's tokens. If your theme looks wrong in a diagram, the
   fix is in `SvgThemer`, not in the theme.

6. **Both modes share the token vocabulary.** There is no "normal
   mode token" and "overseer mode token." A theme that wants to
   differentiate the modes does so with layout and emphasis, not with
   different colours for the same role.

7. **Do not style by widget class name if a token will do.** `QWidget#
   conductorCard { background: @surface0; border: 1px solid @border; }`
   is right. `QWidget#conductorCard { background: #1a1a1a; }` is wrong.

8. **The theme is a coat, not a skeleton.** If your theme needs new
   widgets or new properties to look right, the widget needs a new
   token, not the theme a new rule.

## Dark and light are separate themes

There is no `isDark` flag and no runtime colour inversion. Light and
dark are two complete stylesheets that each define the full token set.
A theme's "dark variant" is a separate theme file.