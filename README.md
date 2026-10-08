# BANKPHONE OS

A phone operating system where money is a system function, not an app.

It runs on the Linux kernel that already ships on the phone. Everything above the kernel (`init`, graphics, UI, money engine, security) is written in C, from scratch. There is no Android and no APK.

**Everything is DEMO / TESTNET. No real money, no real Pix, no real BRL.** Real payments need a licensed partner, and this project is not one.

**Tested on one device only:** Infinix Hot 30i (X669C, MediaTek MT6765H). Anything else needs porting, and flashing the wrong device can make it unusable.

Dev logs: [#1, building a phone OS from scratch in C](https://peakd.com/hive-139531/@devferri/dev-log-1-building-a-phone-os-from-scratch-in-c-and-bricking-my-test-phone-along-the-way) · [#2, the phone bricked itself and the OS runs again](https://peakd.com/hive-139531/@devferri/dev-log-2-my-phone-bricked-itself-and-the-os-im-building-runs-again)

## Before you flash anything

- Flash only the **inactive** A/B slot. The slot you are running from is your way back.
- Never flash an image that has not run yet. The test image uses `bankphone.ro=1`, so it does not write any state.
- Make a full backup of your own partitions first. Nothing here does it for you.
- This is a hobby project and comes with no warranty. You flash at your own risk.

## Install (one command)

You need `zig`, `python3`, `adb` and `fastboot` (Android platform-tools), an Infinix Hot 30i X669C with an **unlocked bootloader**, and the backup above.

Plug the phone in with the USB cable and run:

```bash
./install.sh --dry-run     # does everything except write to the phone
./install.sh               # asks you to type a confirmation before writing
./install.sh --restore     # goes back to the original slot
```

The installer takes the phone to the bootloader by itself, so you do not press any button: from Android it uses `adb reboot bootloader` (USB debugging on), and from a phone that already runs BANKPHONE it sends a command over the USB serial port. That command is only accepted by test images (kernel command line `bankphone.devcmd=1`); a production image accepts no commands.

The installer:

1. detects the phone over USB and refuses anything that is not an X669C;
2. pulls the touch firmware from your own phone (Android only), then reboots to the bootloader;
3. checks that the bootloader is unlocked and that the phone has A/B slots;
4. gets the `boot` image of your phone with `fastboot fetch` (or use `--stock-boot your_boot.img`, taken from your model's official firmware);
5. builds the system and packs a read-only test image;
6. shows a summary and writes only after you type the exact confirmation, to the inactive slot.

No phone data ships in this repository. The kernel, device tree, touch firmware and boot image all come from your own device and stay in `work/` and `rootfs-extra/`, both git-ignored.

If `adb` and `fastboot` are not in your `PATH`, copy `local.env.example` to `local.env` and set the paths.

A phone running an image from before this command existed ignores it. In that case the installer says so and you enter fastboot once by hand (hold Vol+ for 3 seconds on BANKPHONE); the image it flashes accepts the command, and after that it is automatic.

Not verified yet: the serial command on a real X669C (it is tested on the computer and with a simulated device), `fastboot fetch` on a real X669C, and the touch firmware `adb pull` on stock Android. If `fetch` is refused, the installer stops and asks for `--stock-boot`. A slot that fails to boot is not guaranteed to fall back to the original one by itself, so make sure you can reach fastboot.

Manual steps: `tools/get-touch-firmware.sh`, then `./build.sh` (needs `ZIG` and `STOCK_BOOT` in `local.env`), then `bash scripts/pack-test-image.sh`. Without the touch firmware the build works, but the screen does not respond to touch.

## Try it without a phone

Run the tests:

```bash
cc -o /tmp/money_test tests/money_test.c init/money.c -lm && /tmp/money_test
cc -o /tmp/sec_test   tests/sec_test.c   init/sec.c   -lm && /tmp/sec_test
```

The money engine prints `TODOS OK` (39 checks), and the security tests pass 7 checks.

Render the screens to PNG with the same drawing code the phone runs:

```bash
./scripts/host-preview.sh out/
```

## Layout

| Path | Contents |
|---|---|
| `init/` | PID 1, graphics, UI, money engine, security, storage, hardware access, boot diagnostics |
| `tests/`, `init/tests/` | unit tests |
| `tools/` | boot image and ramdisk packers, touch firmware helper |
| `scripts/` | host preview, test image packer, state region hash |
| `install.sh`, `build.sh` | installer and build |

## How it behaves

- The UI always says what is not real: `DEMO`, `NOT AVAILABLE`, `NOT IMPLEMENTED`.
- Amounts are integer cents, never floats.
- The PIN is a salted, iterated SHA-256 (50,000 rounds), not PBKDF2.
- The device exposes a USB serial port and mounts `pstore`, so a boot that reset the phone can be read on the next boot.
- In the safe test mode, state is not saved across reboots.

## License

Apache License 2.0, see [LICENSE](LICENSE) and [NOTICE](NOTICE). The touch firmware, the kernel and the boot images of your device are not part of this repository and are not covered by this license.

---

# BANKPHONE OS (Português)

Um sistema operacional de celular em que o dinheiro é função do sistema, não um app.

Roda no kernel Linux que já vem no aparelho. Tudo acima do kernel (`init`, gráficos, UI, motor de dinheiro, segurança) é escrito em C, do zero. Não há Android nem APK.

**Tudo é DEMO / TESTNET. Não existe dinheiro real, Pix real nem BRL real.** Pagamento real exige um parceiro licenciado, e este projeto não é um.

**Testado em um único aparelho:** Infinix Hot 30i (X669C, MediaTek MT6765H). Qualquer outro precisa de adaptação, e gravar no aparelho errado pode inutilizá-lo.

## Antes de gravar qualquer coisa

- Grave só no slot A/B **inativo**. O slot que você está usando é o seu caminho de volta.
- Nunca grave uma imagem que ainda não rodou. A imagem de teste usa `bankphone.ro=1` e não grava estado.
- Faça antes um backup completo das suas partições. Nada aqui faz isso por você.
- Projeto de hobby, sem garantia. Você grava por sua conta e risco.

## Instalar (um comando)

Você precisa de `zig`, `python3`, `adb` e `fastboot`, de um Infinix Hot 30i X669C com **bootloader destravado** e do backup acima.

Ligue o celular pelo cabo USB e rode:

```bash
./install.sh --dry-run     # faz tudo, menos gravar
./install.sh               # pede uma confirmação digitada antes de gravar
./install.sh --restore     # volta ao slot original
```

O instalador leva o aparelho ao fastboot sozinho, sem você apertar botão: no Android ele usa `adb reboot bootloader`, e em um aparelho que já roda o BANKPHONE ele manda um comando pela porta serial USB (só imagens de teste aceitam esse comando). Um aparelho com imagem antiga, de antes desse comando, precisa entrar no fastboot uma vez na mão (segure Vol+ por 3 segundos); a imagem que o instalador grava já aceita o comando, e dali em diante é automático.

O instalador reconhece o aparelho (só aceita o X669C), pega o firmware do toque e a imagem de `boot` do **seu** celular, compila, mostra um resumo e só grava depois que você digitar a confirmação exata, no slot inativo.

Nenhum dado de celular vem neste repositório: kernel, firmware do toque e imagem de boot saem do seu aparelho. Se `adb` e `fastboot` não estiverem no `PATH`, copie `local.env.example` para `local.env` e ajuste os caminhos.

Ainda não verificado: o `fastboot fetch` em um X669C de verdade e o `adb pull` do firmware em um Android de fábrica. Se o `fetch` for recusado, o instalador para e pede `--stock-boot`. Também não há garantia de que um slot que não sobe volte sozinho para o original.

## Testar sem o celular

Os dois comandos de testes e o `./scripts/host-preview.sh out/`, mostrados acima, funcionam só no computador.

## Licença

Apache License 2.0, veja [LICENSE](LICENSE) e [NOTICE](NOTICE). O firmware do toque, o kernel e as imagens de boot do seu aparelho não fazem parte deste repositório e não são cobertos por esta licença.
