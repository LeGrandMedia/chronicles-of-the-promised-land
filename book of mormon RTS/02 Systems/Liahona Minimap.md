# Liahona Minimap

Every player's brass-framed minimap with moving pointers; not equipment. It rotates with the camera. Failure disables its display/minimap commands, while chat, main camera and actual vision remain usable. Fog/memory: [[Gadianton Plunder and Stealth]].

- Check local chat drafts on edit/paste. Whole-token profanity triggers only on completed punctuation/whitespace or submission; ignore unfinished prefixes/substrings. Normalize case/Unicode; bounded configurable dictionary includes racial slurs, **ass** and **hell**, with Shiz/roster whitelist. Incoming messages never punish recipients. Never log unsent drafts or use an LLM classifier.
- Detection spins/drifts pointers for one second, then blanks map and blocks minimap orders. Reduced-motion skips spinning. Show **“The Liahona has ceased to work.”**; occasional **“Murmuring detected.”** is game wording.
- Only submitted normalized full-content **sorry** restores; ignore case, surrounding punctuation/whitespace. “Not sorry”/“sorry everyone” fail. Typing without submission does nothing. Local apology works even with chat muted.
- Restore legitimate current LOS/memory and aligned pointers. Show **“The Lord acknowledges your repentance.”** only when actually restoring a broken Liahona, superseding “And it came to pass.” No stale live enemy locations, queued extra apologies or Favor refund.
- Original penalty: Nephite Favor −10, at most once per 30 seconds. v0.7 applies −10 to every faction's stat; that extension is an implementation interpretation, not an explicit user change.

Failure persistence across save/load/reconnect remains future work; neither flow exists in v0.8. Draft checks are local with a server submitted-message check for trusted LAN; see [[Developer Handoff]].

Inspiration: Alma 37:38–41; 1 Nephi 16:28–29. Chat profanity/apology mechanics are invented, not scripture quotations.
