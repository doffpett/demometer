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

For å hente en access token programmatisk kan du bruke skriptet `get_tuya_token.js` i dette repoet. Sett miljøvariablene `TUYA_CLIENT_ID` og `TUYA_CLIENT_SECRET` før du kjører skriptet.

```bash
export TUYA_CLIENT_ID=din_client_id
export TUYA_CLIENT_SECRET=din_client_secret
node get_tuya_token.js
```

Skriptet beregner signaturen i henhold til Tuyas dokumentasjon og sender forespørselen til `https://openapi.tuya{region}.com/v1.0/token?grant_type=1`. Regionen kan settes via `TUYA_REGION` (f.eks. `eu`, `us` eller `cn`).

### Miljøvariabler med .env

Du kan også legge inn Tuya-verdiene dine i en `.env`-fil.
Kopier eksempelfilen og fyll inn dine egne nøkler:

```bash
cp .env.example .env
# rediger .env og oppdater TUYA_CLIENT_ID, TUYA_CLIENT_SECRET og eventuelt TUYA_REGION
npm install
node get_tuya_token.js
```

