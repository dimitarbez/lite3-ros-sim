# Семинарска работа за Lite3, EmotionBot и ROS

Главниот документ е `main.tex`. Податоците за студентот и институцијата се
менуваат само во `metadata.tex`.

## Компајлирање

Потребни се XeLaTeX, `fontspec`, `polyglossia`, TikZ/PGF и GNU FreeFont
фонтовите. Семинарската нема посебно поглавје со литература. Целосниот циклус е:

```bash
make
make check
```

Ако на host-системот нема TeX Live, од директориумот `seminarska/` може да се
користи Docker:

```bash
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/xu-cheng/texlive-small:latest sh -lc '
    set -eu
    T=/opt/texlive/texdir/bin/x86_64-linuxmusl
    "$T/tlmgr" update --self
    "$T/tlmgr" install cleveref
    "$T/xelatex" -interaction=nonstopmode -halt-on-error main.tex
    "$T/xelatex" -interaction=nonstopmode -halt-on-error main.tex
    "$T/xelatex" -interaction=nonstopmode -halt-on-error main.tex
  '
```

Ова е и проверениот container build за доставениот PDF. Во TeX Live 2026,
`cleveref` нема македонски language module; `main.tex` затоа задава македонски
имиња за типовите и ги користи стабилните English conjunction rules.

`SOURCE_AUDIT.md` е внатрешна техничка белешка за врската меѓу главните
констатации, кодот, конфигурацијата и датираните записи; не е дел од PDF-от.
Ниту една команда од овој документ не претставува дозвола за физичко
активирање на роботот.
