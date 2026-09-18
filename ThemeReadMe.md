# Themes

Lore reads its colors and styling from stylesheets. Every theme is a
delta on top of the `lore` base theme.

## The `lore` base

`lore` is the base theme. It defines:

- The full palette, as a token block with all twenty-seven tokens.
- Every structural rule the app uses: padding, borders, radii, icon
  references, focus rings, and so on.

`lore` is required. The application refuses to start if it cannot be
loaded. It is not shown in the theme menu. Users cannot select it. Do
not edit it unless you mean to change the base appearance of every
page.

## Everything else is a delta

Any other theme — `normal`, `overseer`, and any you add — is a delta
on top of `lore`. A theme file contains:

1. A `@theme <name>` line.
2. **Only the tokens it wants to override.** Missing tokens inherit
   from `lore`.
3. **Only the stylesheet rules it wants to change.** Missing rules
   inherit from `lore`.

The registry merges tokens. The applied stylesheet is `lore`'s body
concatenated with the theme's body, so the theme's rules win where
they overlap.

A complete, valid theme can be four lines:

```
@theme midnight

@base #0a0a12
@text #e0e0f0
```

That theme sets two colors and inherits everything else from `lore`.

## Adding a theme

**1. Create a folder.**

```
cp -r resources/themes/normal resources/themes/midnight
```

**2. Name it.**

Open `resources/themes/midnight/stylesheet.qss` and set the `@theme`
line to match the folder name:

```
@theme midnight
```

**3. Set the tokens you want to override.**

```
@theme midnight

@base #0a0a12
@mantle #08080e
@text #e0e0f0
@blue #7c8aff
```

Tokens you do not list come from `lore`.

**4. Add rules only if you need to change structure.**

Most themes only need tokens. If you want a rule that `lore` does not
have, or a different version of one it does, write it here. It will
be applied after `lore`'s rules.

```
@theme midnight

@base #0a0a12

QPushButton#primaryButton {
background-color: #7c8aff;
color: #0a0a12;
}
```

**5. Register the file.**

Add one line to `resources/themes/resources.qrc`:

```xml
<file>midnight/stylesheet.qss</file>
```

**6. Rebuild.**

The theme appears in the **Theme** menu under both **Normal** and
**Overseer**. Users can pick it for either page.

## Tokens

The full set of twenty-seven tokens is declared in `lore`. Any theme
can override any subset. The names and their purposes:

```
@base      background of the main window and pages
@mantle    secondary background, one step darker than base
@crust     darkest background, used for menu and status bars
@surface0  panels, editors, list backgrounds
@surface1  hover state, one step lighter than surface0
@surface2  selection state, one step lighter than surface1
@overlay0  muted text, disabled text, dividers
@overlay1  secondary muted text
@overlay2  edge strokes on diagrams, strongest muted tone
@text      body text
@subtext0  secondary text
@subtext1  secondary text, slightly stronger
@blue      primary accent
@lavender  secondary accent
@sapphire  cool accent
@sky       cool accent
@teal      cool accent
@green     success
@yellow    warning
@peach     warm accent
@maroon    strong error
@red       error
@mauve     tertiary accent
@pink      warm accent
@flamingo  warm accent
@rosewater soft warm accent
```

The accent tokens are referenced by role lookups in the code. For
example, `event.user` resolves to `@blue`, and `event.tool.ok`
resolves to `@teal`. Changing a token changes what roles resolve to.

## Rules

- **Do not edit `lore`** unless you mean to change every theme at
  once.
- **`@theme` must match the folder name.** If they disagree, the
  theme registers under the `@theme` name and the menu may not find
  it.
- **The token block must be contiguous and at the top of the file.**
  Blank lines and comments are allowed between tokens, but the block
  ends at the first line that is not a comment, not `@theme`, and not
  `@name #hex`.
- **Do not rely on color alone.** Red and green look similar to
  people with red-green color blindness. Pair color with an icon or
  with text. This is a base rule; it applies to every theme.
- **Qt Style Sheets do not support `box-shadow`, `text-transform`, or
  `letter-spacing`.** These will be silently ignored. If you need
  those effects, they are done in C++ (drop shadow effects,
  uppercased strings), not in QSS.
