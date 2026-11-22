# Known issues and verification gaps

- **Physical LAN is unverified.** Local-process checks do not verify play between computers or across Wi-Fi and router configurations. Test on two computers over UDP 7777 before calling LAN acceptance complete.
- **Host exit ends the match.** There is no host migration or reconnect flow.
- **No saved games or Steam/public-internet service.** This build targets direct private-LAN play and local sandbox use.
- **Rendered performance is unmeasured.** Headless simulation results do not measure GPU frame rate.
- **Audio is limited.** General cues use synthesized fallback audio. The Gadianton Robber has one offline-TTS placeholder WAV, not a recorded voice performance.
- **Unit motion is 2D.** Procedural mesh poses deform the source image and do not reveal hidden surfaces or provide true 3D rotation. Explicit frame art can replace poses or provide death/destruction sequences.
- **Mass casualties prioritize responsiveness.** Up to 256 death effects are retained per player; an extreme burst can omit the oldest cosmetic effects. All casualties still resolve in the simulation.
- **Cosmetic content is local.** Names, `visuals.json`, artwork, and voice assets are not shared over the network; stable catalog IDs control gameplay.

Current checks and limits: [v0.8 validation](Validation-v0.8.md).
