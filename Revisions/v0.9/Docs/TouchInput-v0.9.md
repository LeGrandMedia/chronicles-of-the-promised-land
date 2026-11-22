# v0.9 native touch controls

Recommended and actual model: GPT-6.2 Sol. The Windows Development package passed the rendered touch probe. Native keyboard and cross-platform play still require device verification.

## Interaction

- **Select** lets you tap a unit or building, tap open ground to clear selection, or drag one finger to pan.
- **Orders** lets you tap terrain, a resource, or an enemy after selecting your units. The same target rules apply as for a desktop context order. Drag one finger to pan while Orders is active.
- **Box** lets you drag over your units to select them together. A tap selects a single entity.
- With two fingers on the battlefield, drag both to pan, pinch or spread to adjust zoom, and twist to rotate. Lifting the first finger ends the gesture. The remaining finger cannot dispatch a tap or order.
- Building and other targeted action cards ask you to tap the battlefield. **Cancel** returns to Select and clears any unplaced action or wall endpoint.
- Tap the address in **Join LAN**, the name field in the lobby, or **Chat** to open the platform keyboard. Accepting an address fills the field. **Join** starts the connection.

## Verification

Run the rendered Development build with `-RTSTouchPreview -RTSV09TouchTest -RenderOffscreen -d3d11 -ResX=1280 -ResY=720 -ForceRes -windowed -unattended -nosound`. The probe exercises Unreal viewport touch callbacks and produces `Saved/Tests/v09-touch-tests.json`. Test desktop mouse and keyboard separately with the existing input suite and without `-RTSTouchPreview`.

The 1280×720 and 2400×1080 Windows runs each passed 26 distinct checks. Both moved a real worker on the authoritative simulation, then dispatched a targeted patrol from the HUD. The one-finger and two-finger anchor drift was 0.00000 map meters at reported precision. The pinch changed zoom from 1.6500 to 2.7720, and the twist changed rotation from 0 to 0.2783 radians. [1280×720 touch probe](Evidence/v0.9/input/touch-1280x720/v09-touch-tests.json), [2400×1080 touch probe](Evidence/v0.9/input/touch-final-2400x1080/v09-touch-tests.json)

The 1600×900 Windows desktop probe passed all 67 checks. Shift+wheel rotated from 0 to 0.261799 radians with unchanged zoom. Its four inverse-projection samples and wheel anchor matched at the reported precision. The probe also verified mouse selection, context orders, hotkeys, queues, and six live 3D animation pairs. [Desktop probe results](Evidence/v0.9/input/desktop-rendered-1600x900/input-tests.json)

| Case | Required observation |
| --- | --- |
| Main menu and lobby taps | A captured button changes screens on release. Moving off, a second finger, or a focus flush cancels the tap. Ready toggles exactly once in the presence of synthetic mouse input. |
| Battlefield selection | Tapping a visible unit selects its real ID. Tapping terrain clears selection in Select mode. |
| Context order and precedence | A selected unit receives a movement order from a terrain tap in Orders mode. An explicit targeted card takes precedence. A release over a HUD control cannot issue a world order or activate that control unless its press began there. |
| One-finger pan | Dragging shifts the camera so the original world point follows the finger and leaves selection intact. |
| Box select | The live rect spans the touch path and yields owned units on release. A focus flush clears the live rect and preserves the prior selection. |
| Two-finger transform | New midpoint keeps the original world anchor, span changes zoom, and angle rotates the camera. Lifting one finger and moving or releasing the other preserves selection and performs no order. |
| Mobile text input | On a physical Android and iPhone, use the native keyboard to replace a player name and enter an IPv4 address with optional port. Accept the address, connect to a same-version host, enter the match, and issue a first order. |
| Mobile interruption | On physical devices, briefly background the app while dragging a selection rect and while pointing a build. Resume and verify the rect is gone and the interrupted gesture did not place a structure. |

Unreal 5.3 forwards Android and iOS system touch cancellation as `ETouchType::Ended`. Viewport focus flush and app lifecycle events can abort interrupted gestures separately. Keep physical-device interruption testing distinct from simulated viewport cases.
