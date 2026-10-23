# Demometer firmware 2026-10-23-r6

Stabilitetsversjon for Photon 2/P2 med Device OS 6.3.4 og NeoPixel 1.0.4.
Installerte filer, sjekksummer og verifikasjon ligger i
[`firmware/2026-10-23-r6`](firmware/2026-10-23-r6).

Firmwareen lagrer teller og lysstyrke i EEPROM, begrenser animasjonskøen
og har programvare-watchdog på 60 sekunder samt maskinvare-watchdog på
90 sekunder. Omstart etter 15 minutter sammenhengende skyfrakobling bruker
`RESET_NO_WAIT`. Programvare-watchdog-en bruker nå 1536 byte stack.

`simulate N` legger til N demoer, begrenset til ledige plasser.
`set_fixed N` setter antall tente lys (0–51); pixel 0 er alltid av i tellervisning.
`set_brightness N` begrenses til 0–120. `winning` starter fem sekunder
konfetti; `demo_mode on/off` styrer automatisk simulering.
`reset_state` nullstiller teller/kø, og `ping` returnerer oppetid.

## Live-farge fra webkamera (macOS)

[Live-programmet](live/) leser hovedfargen i et felt øverst i midten av Logitech
BRIO-bildet og setter alle 52 LED-er til denne fargen. Hold boksen nær kameraet,
midt i øvre halvdel av bildet, slik at boksen dekker målefeltet. Dette er
fargeavlesning, ikke gjenkjenning av ølmerke eller objekttype.
Lys, metallreflekser og kameraets hvitbalanse påvirker resultatet. LED-fargene
gjøres mer mettede for å gi tydelige hovedfarger, fremfor en nøyaktig
fotografisk RGB-gjengivelse.

Målefeltet dekker 33–62 % av bredden og 8–42 % av høyden. Fargemettede piksler
vektes høyere enn tekst og svake reflekser. Algoritmen krever en tydelig vinner
blant fargetonene, men tåler at metallboksen har flere nyanser av samme farge.

```sh
python3 -m pip install -r live/requirements.txt
# ffmpeg må være installert, for eksempel med Homebrew.
python3 live/live_color.py start
python3 live/live_color.py status
python3 live/live_color.py stop
```

På denne Macen kan også `Start live.command`, `Stopp live.command` og
`Status.command` i `live/` åpnes fra Finder. Snarveiene bruker Homebrew Python
på `/opt/homebrew/bin/python3`; bruk Python-kommandoene over på andre oppsett.

Programmet er aktivt til det stoppes. Det starter ikke automatisk ved Mac-innlogging.
Macen må være våken, kameraet tilgjengelig og Particle CLI innlogget.
Kamerabildene behandles lokalt i RAM; bare RGB-verdier sendes til Particle.
Fargeendringer sendes tidligst hvert andre sekund etter utjevning av tre bilder.
Ved stabil farge sendes en oppfriskning omtrent hvert femte sekund.
Fargemålingene og diagnoseverdier ligger i `live/.runtime/`, uten bilder eller tokens.

Firmwareen tilbyr `set_color "R,G,B"` og `set_color off`, samt variabelen
`display` (`meter` eller en hex-farge). Fargevisning påvirker ikke demo-telleren.
Stopp sender `off`; ved brudd/avsluttet program går den automatisk tilbake til
vanlig demometervisning etter 15 sekunder uten en gyldig fargeoppdatering.
Live-fargen lagres ikke i EEPROM, og oppstart viser vanlig teller.

Kjør fargetestene med `cd live && python3 -m unittest -v test_color`.

## Bygg og installer

```sh
mkdir -p build
particle compile photon2 . --target 6.3.4 --saveTo build/demometer-photon2.bin
particle get LiveDemoBergen lit_pixels
particle get LiveDemoBergen brightness
particle flash LiveDemoBergen firmware/2026-10-23-r6/demometer-photon2.bin
```

Vent på gjenoppkobling, og bekreft versjon, aktiv watchdog og bevart tilstand:

```sh
particle get LiveDemoBergen fw
particle get LiveDemoBergen hw_wd
particle get LiveDemoBergen lit_pixels
particle get LiveDemoBergen brightness
```

`fw` skal være `2026-10-23-r6`, og `hw_wd` skal være `1`.
Negativ `hw_wd` betyr at Device OS avviste init/start av watchdog.

## Feilsøking ved langvarig drift

| Variabel | Betydning |
| --- | --- |
| `uptime_s` | Oppetid med 64-bits tidsgrunnlag |
| `cloud_ok`, `wifi_ok` | Rapportert sky- og Wi-Fi-forbindelse |
| `wifi_rssi` | Wi-Fi-styrke i dBm; 0 når frakoblet |
| `free_mem`, `min_free` | Ledig minne og laveste måling siden oppstart |
| `offline_s`, `dropouts` | Tid uten sky og antall observerte frakoblinger siden oppstart |
| `reset_rsn`, `reset_data` | Siste omstartsårsak og applikasjonsdata |
| `hw_wd` | 1 når maskinvare-watchdog er startet |

Ved programvarestyrt recovery er `reset_data` 1 for app-watchdog og 2 for
15 minutter uten sky. `reset_rsn=60` betyr hardware-watchdog når plattformen
bevarer reset-info. Photon 2 kan miste reset-info ved enkelte feil.

## Tester

```sh
bash tests/run.sh
```

Testene kjører den faktiske firmwarekoden med simulerte Particle-API-er,
nettverk, EEPROM, klokke og LED-driver, med AddressSanitizer/UndefinedBehaviorSanitizer.
De inkluderer reconnect, offline-reset, 32-bits tidsomslag og 60 simulerte døgn.
De beviser ikke fysisk radio-/strømstabilitet eller at hardware-watchdog resetter
ved alle låsinger. Fysisk langtidstest gjenstår. `particle.ignore` holder
teststubbene utenfor firmwarebyggingen.

---

Prosjektbeskrivelse: Avo Demometer
Bakgrunn
Avo Consulting ønsker å visualisere innsatsen og fremgangen i salgs- og demoinitiativer på en engasjerende måte, både internt og eksternt. For å gjøre dette har vi utviklet et fysisk "demometer" som, med lys og animasjoner, synliggjør hvor mange kundedemoer vi har gjennomført over tid.

Hovedkonsept
Demometeret består av en LED-strip koblet til en Particle.io-enhet. Hver gang en demo logges som aktivitet i vårt CRM-system (Pipedrive), sendes det et signal via Workato (iPaaS) til Particle-enheten. Dette utløser en visuell effekt hvor en "lys-piksel" faller ned på LED-stripen, bygger opp en fargerik søyle og gir umiddelbar feedback på demo-fremdriften.

Funksjonalitet:

Live Demo-teller: Visualiserer antall demoer med et stigende antall lys på LED-stripen.

Automatiske og manuelle triggere: Demoer kan legges til både via automatisering fra CRM eller manuelt for test/moro.

Effekter og milepæler: Lys og animasjoner markerer milepæler og suksess.

Backup og spesialeffekter: Løsningen har en “winning mode” med konfetti/feiring og håndtering av strømbrudd.

Health Check & Robusthet
For å sikre at demometeret alltid viser korrekte og oppdaterte data, innføres det en automatisk “ping”-funksjon. Et eksternt system overvåker om demometeret er online ved jevnlig å sjekke om Particle-enheten svarer. Hvis enheten ikke svarer over tid, tolkes dette som at demometeret er offline eller har hengt seg opp.

Automatisk strømstyring
Dersom demometeret ikke svarer på ping, kuttes strømmen automatisk via en Tuya smart-stikkontakt. Systemet sender et API-kall til Tuya-stikkontakten for å slå av og på strømmen, og forsøker på den måten å restarte demometeret helt automatisk – ingen manuell inngripen nødvendig.

Teknologier
Particle.io – Styrer og oppdaterer LED-strip (demometer).

Neopixel LED-strip – Visualiserer demoer i sanntid.

Pipedrive – CRM-system hvor demoer logges.

Workato – iPaaS for å koble systemene sammen.

Tuya Smart Plug – Styrer strømmen for automatisk omstart/feilhåndtering.

(Planlagt) Health check script – Overvåker om demometeret svarer.

Fordeler
Øker synligheten rundt demoaktivitet og måloppnåelse i teamet.

Gir umiddelbar feedback og ekstra engasjement i hverdagen.

Fullautomatisert oppsett – krever ingen manuell oppfølging for drift og oppdatering.

Kan videreutvikles med flere effekter, farger, dashboards og integrasjoner.

Oppsummering
Avo Demometer er et eksempel på hvordan fysiske, digitale og skybaserte løsninger kan spille sammen for å skape både innsikt, synlighet og engasjement på arbeidsplassen – hele veien fra CRM, via automasjon, til fysisk visualisering og selvhelbredende teknologi.

## Hente access token fra Tuya med Node.js

For å hente en access token programmatisk kan du bruke skriptet `get_tuya_token.js` i dette repoet. Miljøvariablene kan legges i en `.env`-fil. Kopier først eksempelfilen og fyll inn dine verdier:

```bash
cp .env.example .env
# rediger .env og legg inn dine Tuya-verdier
node get_tuya_token.js
```

Du kan også sette variablene direkte i miljøet før du kjører skriptet:

```bash
export TUYA_CLIENT_ID=din_client_id
export TUYA_CLIENT_SECRET=din_client_secret
node get_tuya_token.js
```

Skriptet beregner signaturen i henhold til Tuyas dokumentasjon og sender forespørselen til `https://openapi.tuya{region}.com/v1.0/token?grant_type=1`. Regionen kan settes via `TUYA_REGION` (f.eks. `eu`, `us` eller `cn`).

## Kalle Tuya API med Node.js

Når du allerede har en `access_token`, kan du bruke skriptet `tuya_request.js` til å sende egne API-kall. Skriptet tar HTTP-metoden og stien (med evt. query-parametre) som argumenter.

Sett miljøvariablene `TUYA_CLIENT_ID`, `TUYA_CLIENT_SECRET` og `TUYA_ACCESS_TOKEN` først. `TUYA_REGION` kan også settes dersom du bruker et annet område enn `eu`.

```bash
export TUYA_CLIENT_ID=din_client_id
export TUYA_CLIENT_SECRET=din_client_secret
export TUYA_ACCESS_TOKEN=din_access_token
node tuya_request.js GET /v1.0/devices
```

Du kan også sende en JSON-body ved å oppgi den som tredje argument. Signeringen skjer automatisk basert på parametrene du oppgir.
