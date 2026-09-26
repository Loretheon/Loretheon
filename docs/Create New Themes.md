# Creating Themes for Lore

## Where themes live

Every theme is a folder under `resources/themes/`:

```
resources/themes/
default/           ← the shipped light theme
stylesheet.qss
default-dark/      ← the shipped dark theme
stylesheet.qss
your-theme-name/
stylesheet.qss
```

To add a theme, create a new folder with a single `stylesheet.qss`
inside it, and add the folder to the Qt resource file (`resources.qrc`)
under the same prefix. At startup, `ThemeRegistry` scans the resource
prefix and registers every theme it finds. Registered themes appear in
the **Theme** menu under both **Normal** and **Overseer**.

## The stylesheet format

A theme is a normal Qt stylesheet (`.qss`) with two additions at the
top: a theme name and a token block. Everything after the token block
is ordinary QSS.

```qss
@theme my-theme

@base          #101014
@surface0      #18181f
@surface1      #22222c
@surface2      #2c2c38
@surface-raised #26262f
@structure     #0a0a0e

@text          #e8e8ee
@text-muted    #a0a0ac
@text-subtle   #6c6c78
@text-disabled #44444c

@accent        #7aa2f7
@accent-hover  #90b4ff
@accent-pressed #5f88d8
@accent-muted  #3a4a6a
@accent-fg     #0a0a0e

@hint-cool     #7dcfff
@hint-warm     #e0af68
@hint-neutral  #9aa5ce

@border        #2c2c38
@border-strong #3a3a4a
@divider       #22222c

@success       #9ece6a
@warning       #e0af68
@error         #f7768e
@info          #7aa2f7

/* ---- ordinary QSS follows ---- */

QWidget {
  background: @base;
  color: @text;
}

QPushButton {
  background: @surface1;
  color: @text;
  border: 1px solid @border;
  border-radius: 4px;
  padding: 4px 10px;
}

QPushButton:hover {
  background: @surface2;
  border-color: @border-strong;
}

QPushButton:focus {
  border-color: @accent;
}

QLineEdit, QPlainTextEdit, QTextEdit {
  background: @surface0;
  color: @text;
  border: 1px solid @border;
  border-radius: 4px;
}

QLabel[muted="true"] {
  color: @text-muted;
}
```

### Rules for the token block

- One token per line, `@name value`, no quotes around the hex.
- `@theme <name>` must appear before the first token line.
- A blank line or the first non-`@` line ends the token block.
- The block must be at the very top of the file. Comments above it are
  allowed; anything else is not.

### Rules for the QSS body

- Always reference tokens (`@text`, `@accent`), never literal hex.
- Never set a colour on a widget that has a token for its role.
- If a widget needs a colour that no token provides, open an issue — a
  new token is added to the vocabulary, not to the theme.

## A minimal theme

The smallest valid theme defines all tokens and no widget rules. It will
inherit the default look with your colours:

```qss
@theme minimal

@base #1e1e2e
@surface0 #181825
@surface1 #313244
@surface2 #45475a
@surface-raised #313244
@structure #11111b

@text #cdd6f4
@text-muted #a6adc8
@text-subtle #7f849c
@text-disabled #585b70

@accent #89b4fa
@accent-hover #b4befe
@accent-pressed #74c7ec
@accent-muted #45475a
@accent-fg #1e1e2e

@hint-cool #89dceb
@hint-warm #fab387
@hint-neutral #bac2de

@border #313244
@border-strong #45475a
@divider #313244

@success #a6e3a1
@warning #f9e2af
@error #f38ba8
@info #89b4fa
```

## Previewing

Save your theme folder under `resources/themes/`, add it to
`resources.qrc`, rebuild, and open the **Theme** menu. Select your
theme from the **Normal** submenu to check the editor, and from the
**Overseer** submenu to check the conductor board, transcript, and
workstation.

Check both:

- **Normal mode**: file tree, editor, preview pane, chat panel,
  diagram renderers.
- **Overseer mode**: conductor dock, kanban columns, cards, graph
  panel, roster strip, transcript, workstation windows, toasts.

## Opening a PR

1. Fork the repository.
2. Add `resources/themes/<your-theme>/stylesheet.qss`.
3. Add the folder to `resources.qrc` under the same prefix as the
   other themes.
4. Run the app in both modes and confirm every widget is legible.
5. Open a PR. Include a screenshot of both modes in the description.
6. State the theme's intent: is it a dark theme, a light theme, a
   high-contrast theme, a specific palette (Gruvbox, Catppuccin,
   Nord, …)?

Themes that only change a few colours from an existing theme should
be marked as such and will be reviewed against the original.

## Checklist before submitting

- [ ] Folder name is lowercase, hyphenated, no spaces.
- [ ] `@theme` line matches the folder name.
- [ ] Every token is defined.
- [ ] No hex literals below the token block.
- [ ] Body text meets 4.5:1 contrast on `base`, `surface0`, `surface1`.
- [ ] `accent` and `accent-fg` are mutually readable.
- [ ] Status colours are only used for their semantic role.
- [ ] Tested in both Normal and Overseer modes.
- [ ] Screenshot of both modes in the PR description.