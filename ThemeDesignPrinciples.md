# Lore theme design principles

Lore's colors are a system, not a palette. This document explains the
system so that anyone editing a theme, adding a widget, or reviewing a
pull request can make decisions that keep the app coherent.

## The model

Three things, in order:

1. **Tokens.** Named colors with a role. `@accent`, `@text`, `@error`.
2. **The theme file.** A token block. It declares a value for every
   token. Nothing else.
3. **The stylesheet body.** Rules that reference tokens. It never
   names a specific color.

The theme file is the only place a hex value appears. The body is the
same across all themes. A new theme is a new token block; the body is
never touched.

## What the tokens mean

There are twenty-five tokens. They divide into six groups.

### Surfaces

Backgrounds and panels. From darkest to lightest.

- `@base` — the page. Every window, dialog, and frame defaults to it.
- `@surface0` — panels and inputs that need to sit above the page.
- `@surface1` — one step above `@surface0`. Hover states, table rows.
- `@surface2` — one step above `@surface1`. Selection states.
- `@surface-raised` — the surface for floating elements: menus,
  tooltips, popovers. Usually the same as `@surface0` or `@surface1`.
- `@structure` — the deep structural color. Used for window borders
  and the darkest chrome. Rarely used as a background.

### Text

- `@text` — body text. The default color for readable content.
- `@text-muted` — secondary text. Labels, captions, metadata.
- `@text-subtle` — tertiary text. Timestamps, hints, very quiet UI.
- `@text-disabled` — text on disabled widgets. Must still be readable
  enough to identify the widget, but must not compete with `@text`.

### Accent

The interactive color. There is one accent family, and it appears in
five states.

- `@accent` — the default accent. Primary buttons, focus rings,
  selected tabs, links.
- `@accent-hover` — the accent, one step lighter. Button hover,
  link hover.
- `@accent-pressed` — the accent, one step darker. Button press,
  active state.
- `@accent-muted` — the accent, desaturated. Structural uses: thick
  borders, badges that need to read as accent without shouting.
- `@accent-fg` — foreground text placed on top of `@accent`. Usually
  the theme's own base color, so that text on a filled accent button
  reads as the page showing through.

### Hints

Three soft colors for states that should read as related to the
accent family but distinct from it. Used for tags, badges, proposal
cards, and any element that wants to be a soft color without being
the accent.

- `@hint-cool` — a soft blue. Info states, cool tags.
- `@hint-warm` — a soft red. Warm tags, secondary errors.
- `@hint-neutral` — a soft neutral. The default hint for anything
  that needs a soft color but no particular direction.

These are the only colors in the palette that lean blue or red. The
lean is subtle: a cool hint is a bluish purple, not a blue. A warm
hint is a reddish purple, not a red.

### Borders and dividers

- `@border` — the default border. Inputs, panels, chrome.
- `@border-strong` — a stronger border for elements that need to
  read as structurally important. Focused windows, active panels.
- `@divider` — a very light line. Separators between rows, thin
  rules inside panels.

The difference between `@border` and `@divider` is weight, not color.
Use `@divider` for one-pixel lines that separate content, and
`@border` for the outline of an element.

### States

Four colors for status. These are the only tokens that sit outside
the accent family on purpose. They need to be distinguishable from
the accent and from each other.

- `@success` — positive states. Confirmation, completion, healthy.
- `@warning` — caution states. Attention needed, but not an error.
- `@error` — error states. Failure, destructive actions, invalid
  input.
- `@info` — informational states. Neutral messages, hints, help.

## Rules

### 1. The body does not name colors

Every rule in the stylesheet references a token. No hex values in
the body. If a rule needs a color that no token provides, the answer
is either to use the closest existing token or to add a token to
the vocabulary — not to write a one-off hex.

### 2. Colors signal, they do not decorate

A color exists to carry meaning. `@success` is green because success
reads as green to most people. `@accent` is the interactive color and
is used only on interactive elements. Do not use `@accent` to color
a panel just because it looks nice; the accent means "you can
interact with this."

### 3. Never rely on color alone

Every state must be readable by shape or text as well. Red and green
are indistinguishable to roughly 8% of men. An error indicator is
an error because it has an error icon, an error message, and an
error color — not because it is red.

This applies to:

- Success and error states. Pair with a checkmark and an X.
- Warning states. Pair with a triangle.
- Selected states. Pair with a border or a bold weight, not only a
  background change.
- Focus rings. They are visible because they are a ring, not only
  because they are colored.

### 4. Accent is rare

`@accent` is the loudest color in the theme. It should appear on:

- The one primary button in a dialog.
- The focus ring on the currently focused input.
- The active tab.
- The selected row in a list.
- A link.

That is roughly it. If the accent appears on more than ten percent
of the visible surface at any given moment, the interface is too
loud. Pull it back.

### 5. Surfaces step, they do not jump

`@base`, `@surface0`, `@surface1`, `@surface2` form a ramp. The
difference between adjacent surfaces should be small enough that
they read as a family and large enough that the eye can tell which
is on top of which. If two adjacent surfaces look identical, the
ramp is too shallow. If they look like different themes, it is too
steep.

The ramp is defined by the theme, not by the body. A theme that
wants a flatter look can compress the ramp; a theme that wants more
depth can stretch it.

### 6. Text contrast is not negotiable

Every text token must have a contrast ratio of at least 4.5:1 against
the surface it appears on. `@text` on `@base` is the primary pair.
`@text-muted` on `@base` is the secondary pair. Both must pass.

`@text-disabled` may fall below 4.5:1 because disabled text is
decorative — but it must still be readable enough that the user can
identify the widget. 3:1 is a reasonable floor.

### 7. Hints are soft, accents are not

A hint is desaturated and pastel. An accent is saturated and direct.
If a hint reads as an accent, it is too saturated. If an accent reads
as a hint, it is too pale.

The purpose of the two is different. A hint says "this belongs to a
group." An accent says "click me." Do not use one for the other.

### 8. States are outside the family

`@success`, `@warning`, `@error`, and `@info` are not purple. They
may be tints that harmonize with the purple family, but they must be
recognizably their own hue. If `@error` reads as a red-leaning
purple, the user cannot tell an error from a warm hint.

## What a theme file looks like

A theme declares every token. In any order. The body is not repeated.

```
@theme normal

@base           #171624
@surface0       #1F1D30
@surface1       #29263E
@surface2       #34304C
@surface-raised #1F1D30
@structure      #4A3A8F

@text           #ECE8F5
@text-muted     #B0A4C8
@text-subtle    #8274A0
@text-disabled  #60547A

@accent         #8B71FF
@accent-hover   #9F8BFF
@accent-pressed #7B61FF
@accent-muted   #7B61FF
@accent-fg      #171624

@hint-cool      #B0C0F0
@hint-warm      #F0B0C8
@hint-neutral   #C0B0E8

@border         #4A3A8F
@border-strong  #7B61FF
@divider        #3A2E5F

@success        #7BC67B
@warning        #E8C76B
@error          #E05C7A
@info           #B0C0F0
```

The `@theme` line names the theme. Every other line declares a token.
The theme is rejected if any token is missing.

## What a body rule looks like

```
QPushButton#primaryButton {
background-color: @accent;
color: @accent-fg;
border: 1px solid @accent;
border-radius: 4px;
padding: 6px 14px;
font-weight: 600;
}

QPushButton#primaryButton:hover {
background-color: @accent-hover;
border-color: @accent-hover;
}

QPushButton#primaryButton:pressed {
background-color: @accent-pressed;
border-color: @accent-pressed;
}
```

Every value is a token. The rule works unchanged in every theme,
because every theme declares the same token names.

## What is not in this system

- **No ramps.** There is no hue rotation, no lightness curve, no
  computed color. Every token is a hex value chosen by a human.
- **No inheritance between themes at the token level.** Every theme
  declares every token. `normal` is not `lore` with overrides; it is
  a full set of values that happens to look similar.
- **No `@` variables that are not tokens.** `@accent` is a token.
  `@mauve` is not. If a name is not in the token list above, it does
  not exist.
- **No per-widget colors.** A widget is styled by tokens. If a widget
  needs a color that no token provides, the vocabulary is wrong, not
  the widget.

## A checklist for a new theme

Before you submit a theme, verify:

- [ ] The `@theme` line matches the folder name.
- [ ] All twenty-five tokens are declared.
- [ ] `@text` on `@base` has contrast of at least 4.5:1.
- [ ] `@text-muted` on `@base` has contrast of at least 4.5:1.
- [ ] `@accent` on `@base` is visually distinct from `@hint-neutral`.
- [ ] `@error` is not confusable with `@hint-warm`.
- [ ] `@success` and `@error` are distinguishable to someone with
      deuteranopia.
- [ ] The four surfaces form a visible ramp.
- [ ] The body is unchanged from the base theme, except for rules
      for widgets unique to this theme.

If any of these fail, the theme is not ready.

## A checklist for a new widget

Before you style a new widget, verify:

- [ ] Every color it uses is an existing token.
- [ ] If it uses `@accent`, it is interactive.
- [ ] If it uses a state color, it also uses an icon or a label.
- [ ] If it uses `@hint-*`, it is a soft element, not an accent.
- [ ] It has a focus state, and the focus state is visible without
      color alone.
- [ ] It has a disabled state, and disabled text uses
      `@text-disabled`.
