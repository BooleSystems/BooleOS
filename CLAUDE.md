## Regras de verificação

- NUNCA rodar QEMU automaticamente para verificar boot ou capturar
  screenshot
- NUNCA usar sleeps longos (>2s) em comandos de verificação
- Quando precisar confirmar que o kernel bootou corretamente, ou
  testar qualquer comportamento do shell/editor/syscalls, apenas
  diga o comando exato (ex: "cd tools && make run", depois os
  comandos de shell a digitar) e aguarde o usuário rodar e reportar
  o output. O usuário vai colar o resultado manualmente.
- Depois de qualquer mudança de código, SEMPRE rode "make clean &&
  make" (não só "make") antes de pedir pro usuário testar, e
  confirme no output que os arquivos relevantes foram realmente
  recompilados (procure a linha "CC ../kernel/arquivo.c" no log de
  build). Um "make" sem "clean" pode silenciosamente não recompilar
  se a dependência não estiver corretamente declarada no Makefile,
  fazendo o usuário testar um binário antigo sem saber.
- Nunca assuma que um teste "passou" ou "falhou" sem ver o output
  real colado pelo usuário — não infira do fato de ter compilado sem
  erro que o comportamento em runtime está correto.

## Definition of Done (checklist obrigatório — leia antes de commitar e antes de fechar fase)

Duas coisas foram esquecidas sistematicamente em fases anteriores, e de novo na
Fase 19: a entrada de `CHANGELOG.md` em `[Unreleased]` e o rastro de pendência
em `docs/TODO.md`. A fase era implementada e testada direitinho, e o
encerramento (changelog e doc) era pulado sem ninguém perceber. Por isso esta
seção é um checklist literal, não prosa: cada item precisa estar de fato
cumprido, nenhum é opcional, e nenhum pode ser pulado em silêncio. As seções
"Documentação", "Registro obrigatório no CHANGELOG" e "Convenções de fim de
fase" abaixo detalham COMO; esta seção diz O QUE precisa estar feito e QUANDO.

### A) Durante o desenvolvimento — em cada commit que fecha uma subtarefa real
(não em cada linha alterada)

- [ ] Se a documentação técnica formal daquilo ainda não existe, deixar rastro
  mínimo em `docs/TODO.md`, num destes dois formatos:
  ```
  WIP: documentar <feature> (arquivos: caminho/a.c, caminho/b.h)
  TODO later doc. Related files: caminho/a.c, caminho/b.h
  ```
  Obrigatório MESMO quando parece óbvio o que foi feito — quem escreve o código
  tem o contexto mais fresco, e recuperá-lo depois é caro.
- [ ] Deixar uma entrada em `CHANGELOG.md`, seção `[Unreleased]` (Added / Changed
  / Fixed), descrevendo a subtarefa. Vale para toda tarefa que edita qualquer
  arquivo do repositório, inclusive doc pura. Não deixar "pro polish".

### B) No commit de polish final de qualquer fase (o que fecha a fase, antes do merge `nightly` -> `main`)

Todos os itens abaixo, nenhum pulado silenciosamente:

- [ ] **CHANGELOG.md — item BLOQUEANTE, o mais esquecido até agora.** A seção
  `[Unreleased]` contém, de verdade no arquivo, UMA entrada consolidada
  (Added/Changed/Fixed) cobrindo a fase inteira — não uma entrada por subfase, e
  sem narrar bug que nasceu e morreu dentro da própria fase — e ela é renomeada
  para `## [X.Y.Z]` no fechamento. **A fase NÃO está fechada sem essa entrada
  existir no arquivo.** Confirmar abrindo o arquivo, não por lembrança.
- [ ] README.md atualizado (só fase inteira fechada aparece; banner ASCII do topo
  editado à mão; lista de arquivos e de docs em dia).
- [ ] ROADMAP.md sincronizado (tabela granular, seção da fase, dependências).
- [ ] `kernel/version.h` atualizado (sufixo `-nightly` removido, fase e descrição).
- [ ] Toda pendência de `docs/TODO.md` referente à fase virou documentação de
  verdade nos `docs/*.md` corretos e SAIU do `TODO.md` (o arquivo fica só com os
  cabeçalhos quando a versão fecha).
- [ ] PROGRESS.md atualizado (fase atual, dívidas técnicas, próxima fase).
- [ ] `docs/syscalls.md` conferido contra `kernel/syscall.h` (ver seção de
  números de syscall).
- [ ] Selftest 100% passando (o usuário roda no QEMU e cola a saída).

**Antes de declarar qualquer fase fechada, releia esse checklist item por item
contra o estado real dos arquivos, não contra a lembrança do que foi feito.**

## Debug e instrumentação temporária

- Prints de debug adicionados durante investigação DEVEM escrever
  SÓ na serial (serial_putchar/serial_puts/serial_u32/etc), NUNCA em
  VGA (vga_putchar/vga_puts), a menos que explicitamente pedido o
  contrário. O comando "edit" do shell desenha sua própria UI direto
  na tela VGA (barra de status, cursor, texto), e qualquer print de
  debug em VGA se mistura visualmente com a interface e quebra o
  teste. Isso já aconteceu múltiplas vezes nesse projeto.
- Atenção: vga_putchar() já espelha automaticamente cada caractere
  pra serial. NUNCA escreva manualmente em serial DEPOIS de chamar
  uma função que já usa vga_puts()/vga_putchar() internamente — isso
  duplica a saída na serial (já causou bug de "print duplicado" que
  pareceu, por engano, sintoma de syscall rodando duas vezes). Essa
  mesma regra vale, no futuro, para console_putc() e para msg() (ver
  seções de camadas de abstração e idioma/saída de texto abaixo): o
  espelhamento pra serial deve viver dentro da função abstrata,
  nunca duplicado manualmente por cima dela.
- Sempre que investigar um bug, prefira contadores/instrumentação
  cirúrgica (ex: contador global de chamadas, print do valor exato
  numa função específica) em vez de reescrever lógica "no escuro".
  Reportar os valores encontrados ANTES de aplicar qualquer fix.
- Ao final de uma investigação, remova TODOS os prints de debug
  temporários adicionados durante ela, mas preserve 100% da lógica
  de correção. Rode "make clean && make" pra confirmar que a remoção
  não quebrou o build, e liste explicitamente quais arquivos tiveram
  debug removido.
- Nunca conclua "não há bug" sem antes reproduzir o cenário exato
  reportado (inclusive em disco/estado limpo, se relevante — ex:
  "rm -f build/disk.img && make disk" antes de testar bugs de FAT16).

## Convenções técnicas

- C99 estrito, sem libc (bare metal)
- NASM para assembly
- Sem alocação dinâmica sem passar pelo heap manager já existente
  (kmalloc/kfree)
- Preservar compatibilidade com FAT16/VFS já implementado
- Qualquer mudança em scheduler, interrupts, ou timing de hardware
  (ex: probing de dispositivos ATA) merece atenção redobrada com
  race conditions — nunca assumir que um delay fixo é suficiente
  onde um polling real de status é o correto (ex: bug histórico do
  probe ATA usando delay fixo de 400ns em vez de polling do bit BSY)
- Cuidado com arrays de tamanho fixo em structs de disco (ex:
  name[8]/ext[3] separados em dirents FAT16): nunca indexar além do
  tamanho declarado do array mesmo que o layout de memória pareça
  contíguo — isso é undefined behavior em C e já causou bugs onde o
  print de debug mostrava valores corretos mas a comparação lógica
  falhava (o compilador pode gerar código diferente para leitura
  fora dos limites declarados). Sempre construir um buffer explícito
  do tamanho certo antes de comparar/combinar campos assim.
- Cuidado com lógica duplicada: se uma função de busca/comparação
  (ex: fat16_find/dir_lookup) tem uma cópia inline equivalente em
  outro lugar do código, um bug corrigido numa cópia não é
  automaticamente corrigido na outra. Ao corrigir um bug desse tipo,
  procure por padrões idênticos em outros arquivos antes de
  considerar o problema resolvido.
- Funções que fazem I/O de baixo nível (ex: ata_write_sector) devem
  distinguir claramente "operação principal falhou" de "confirmação/
  flush subsequente falhou" — não retornar erro genérico que faça o
  chamador de mais alto nível (fat16_create, etc) pensar que nada foi
  escrito quando na verdade foi.
- Ao adicionar um parâmetro que muda o comportamento de uma cadeia de
  chamadas inteira (ex: cwd_cluster na Fase 15, stdin_redirect/
  stdout_redirect na Fase 16), threade-o explicitamente por toda a
  cadeia (syscall → exec()/fork() → scheduler_spawn_* →
  process_spawn_*) em vez de assumir herança implícita. exec() em
  NullOS NÃO substitui a imagem do processo chamador como o exec()
  POSIX — ele cria um process_t inteiramente novo e independente via
  process_spawn_user(), então nenhum campo do processo chamador
  chega automaticamente lá; só fork() copia campos diretamente
  pai→filho, e mesmo assim cada campo precisa ser copiado
  explicitamente em process_fork(), nunca por "efeito colateral". Ao
  planejar qualquer feature nova que dependa de estado do processo
  (cwd, redirecionamento, variável de ambiente futura, etc.),
  verifique explicitamente se ela precisa passar tanto pelo caminho
  de fork() quanto pelo de exec() antes de implementar — isso já
  gerou um bug real (Fase 15: cwd_cluster só tinha sido pensado para
  fork(), exec() sempre resetava pra root) e quase gerou outro
  (Fase 16: o plano original de pipes assumia fork()+exec() ao estilo
  POSIX pro shell lançar comandos de pipeline, o que teria criado
  processos a mais e nunca propagado os redirecionamentos, porque
  exec() aqui não substitui processo nenhum).
- Qualquer código que rode como parte do Safe Mode (ver seção
  dedicada abaixo) NUNCA pode depender de process_spawn_user/fork/exec ou
  de qualquer coisa que passe pelo scheduler — o propósito do Safe
  Mode é sobreviver a bug justamente nesses subsistemas.

## Camadas de abstração de hardware (HAL)

- Princípio geral: uma função de acesso a hardware vira candidata a
  abstração (ex: `console_putc()`, `input_poll_key()`,
  `block_read_sector()`/`block_write_sector()`, `power_reboot()`/
  `power_shutdown()`, `boot_get_memory_map()`) quando pelo menos UMA
  destas condições é verdadeira: (a) o código por trás dela já vai
  ser reescrito de qualquer forma nesta mesma tarefa (custo de
  embrulhar é ~zero), ou (b) já existe uma segunda implementação real
  e concreta esperada em breve (não hipotética).
- NÃO abstrair "no escuro": se só existe UMA implementação e nenhuma
  segunda está prevista com data/fase concreta (ex: PIC→APIC sem SMP
  no roadmap, PIT→RTC sem necessidade de hora real, porta serial sem
  uso real além de espelho de debug, PCI legacy I/O vs MMCONFIG), NÃO
  crie a interface abstrata ainda — o risco de errar o formato da
  interface por adivinhação supera o benefício. Espere a segunda
  implementação real aparecer.
- Toda função HAL nova deve ser puramente C, arquitetura-neutra na
  assinatura — a parte específica de arquitetura fica escondida
  dentro da implementação, nunca vazando pro chamador. Isso importa
  em especial pro dia de um porte pra outra arquitetura (ex: ARM não
  tem Multiboot2/porta I/O/PIC — só a lógica C de alto nível
  sobrevive nesse porte, não a implementação por trás da função HAL).

## Idioma do código e saída de texto

- A partir da v0.10.1, TODO código novo (comentários, mensagens de
  boot, strings de erro, texto de ajuda, nomes de identificadores)
  deve ser escrito em INGLÊS, sem exceção — não volte a introduzir
  português em nenhum arquivo novo ou editado.
- Isso vale mesmo que um arquivo existente ainda tenha trechos em
  português que não foram tocados numa tarefa específica: ao editar
  qualquer parte de um arquivo, aproveite pra traduzir comentários/
  strings próximos que ainda estejam em português, se estiver dentro
  do escopo razoável da tarefa.
- Prompts do usuário no chat de planejamento continuam em português
  (isso não muda), só o código/output do sistema em si é que deve
  ser 100% inglês daqui pra frente.
- **Saída de texto centralizada (a partir da fase que introduzir a
  camada de abstração de hardware):** toda string que aparece pra
  quem usa o sistema (mensagem de boot, erro de shell, texto de menu
  do Safe Mode, etc.) deve passar por um ponto central (ex:
  `msg(MSG_ID)`) em vez de string literal solta espalhada em
  printf/vga_puts direto no meio do código. Isso NÃO é um sistema de
  tradução — a tabela tem hoje uma coluna só, em inglês, que é o
  idioma padrão e único do NullOS por tempo indeterminado (o sistema
  já é 100% inglês, isso é só mover o texto que já existe pra dentro
  de uma tabela central, não traduzir nada). O único objetivo agora é
  ter um ponto único de saída, barato de fazer cedo (é puro
  reposicionamento do que já existe) e caro de fazer tarde (string
  nova se espalha a cada fase). Não construa suporte a segundo
  idioma, seletor de idioma, nem coluna extra na tabela até haver
  pedido explícito disso — sem necessidade real confirmada, isso é
  custo recorrente em toda fase futura sem benefício correspondente.

## Documentação

- Estrutura de documentação (a partir da reorganização pós-Fase 14):
  README.md é um ÍNDICE ENXUTO (overview curto, banner de versão,
  tabela de fases JÁ CONCLUÍDAS, links pra ROADMAP.md e pra cada
  docs/<assunto>.md, instruções de build, license) — não repete
  conteúdo técnico detalhado. ROADMAP.md guarda o planejamento de
  fases FUTURAS (ainda não concluídas), com goal/approach/risco/
  dependências de cada uma. docs/<assunto>.md (um arquivo por sistema
  técnico, ex: docs/memory.md, docs/scheduler.md, docs/syscalls.md,
  docs/filesystem.md, docs/security.md, docs/pci.md, docs/shell.md,
  docs/kernel.md, docs/pipes.md, docs/testing.md, docs/setup.md)
  guarda a descrição detalhada de cada sistema — é aqui que vai o "o
  que foi implementado" de uma feature nova, não no README.
- **ROADMAP.md ganha uma tabela granular** (a partir da introdução da
  branch `nightly`, ver seção dedicada abaixo): uma linha por fase E
  por subfase já concluída (incluindo retroativamente fases
  históricas com letra, ex: Fase 2/2b, 3a/3b), apontando pra
  docs/*.md e CHANGELOG.md como fonte de detalhe — sem duplicar
  parágrafo técnico nessa tabela, só nome + link.
- **(Item obrigatório da "Definition of Done", A — não é opcional.)
  Documentação escrita DURANTE o desenvolvimento em `nightly`, não
  só no polish final:** toda mudança de código relevante deixa, no
  mínimo, um ponteiro/stub em docs/TODO.md (ou seção equivalente
  centralizada) tipo "WIP: documentar <feature> (arquivos: X, Y, Z)"
  — não precisa estar 100% completo a cada commit, mas não pode
  ficar sem nenhum rastro até o polish final. Motivo: quem escreveu o
  código tem o contexto mais fresco; recuperar esse contexto depois
  é caro.
- SEMPRE que uma fase/feature nova for concluída e eu confirmar que
  o teste passou, atualize a documentação como parte da MESMA tarefa
  (não espere um pedido separado): banner de versão e tabela de fases
  concluídas no README.md; o docs/<assunto>.md relevante (crie um
  novo arquivo se a feature não se encaixa em nenhum existente, e
  linke-o a partir do README) com a descrição detalhada da feature;
  docs/syscalls.md se adicionou syscall nova; lista de
  arquivos/estrutura no README se adicionou arquivo novo; e mova a
  entrada correspondente de ROADMAP.md pra tabela de concluídas do
  README se a fase estava lá planejada.
- Isso vale mesmo que o pedido original não mencione o README ou
  docs/ explicitamente — a atualização da documentação é parte
  implícita de "fase concluída", não uma tarefa separada que precisa
  ser pedida.
- Nunca invente um novo arquivo docs/ ou uma reorganização de
  documentação por conta própria fora desse fluxo de fim-de-fase —
  se não estiver claro em qual docs/<assunto>.md uma informação nova
  deveria entrar, pergunte antes de decidir.
- Antes de escrever/atualizar qualquer trecho de documentação técnica
  (comandos de build, setup, arquitetura, toolchain), verifique
  contra o código/scripts reais em vez de assumir que o texto antigo
  ainda está certo — esse projeto já teve múltiplas rodadas de docs
  desatualizadas (arquitetura de cross-compiler errada em setup.md,
  ferramenta de ramfs referenciada como "a implementar" muito depois
  de já existir, tamanho de PMM divergente da constante real, comando
  make inexistente documentado como se existisse, script que não
  anexa disco sendo confundido com o que anexa). Ao fazer uma
  auditoria de documentação, confira cada afirmação técnica contra o
  arquivo fonte relevante (Makefile, script, header) antes de
  declarar a doc correta.

## Memória de trabalho (PROGRESS.md)

- TODA sessão nova deve ler PROGRESS.md ANTES de começar qualquer
  tarefa. Ele existe porque a sessão não tem memória do que foi
  decidido ou descoberto em sessões anteriores além do que está no
  código e no git log — PROGRESS.md preenche essa lacuna com
  decisões de arquitetura não-óbvias e dívidas técnicas conhecidas.
- PROGRESS.md deve ser atualizado como parte da MESMA tarefa sempre
  que uma decisão de arquitetura nova for tomada, ou uma dívida
  técnica nova for identificada ou resolvida — mesma convenção já
  aplicada ao README.md pra fases concluídas (não é uma tarefa
  separada que precisa ser pedida).
- PROGRESS.md também deve refletir, a qualquer momento, qual subfase
  (letra) da fase atual está em andamento — já que uma fase agora
  pode levar vários pushes de `nightly` antes de fechar (ver seção de
  branch abaixo).
- PROGRESS.md não deve duplicar conteúdo do README.md/ROADMAP.md/
  docs/ (descrições de fase, tabelas de syscall, etc.) — só
  referenciar o arquivo/seção relevante. README/ROADMAP/docs
  continuam sendo a documentação pública do projeto; PROGRESS.md é
  memória de trabalho interna pra sessões futuras.
- Se PROGRESS.md passar de ~200-300 linhas, a próxima sessão que
  notar isso deve consolidar antes de adicionar mais conteúdo:
  dívidas técnicas já resolvidas devem ser removidas (não empilhadas
  como histórico — isso é o que o git log já serve pra registrar), e
  decisões de arquitetura antigas mas ainda relevantes devem ficar
  mais concisas em vez de acumular indefinidamente.

## Estratégia de branch

- `main` = sempre só a última versão lançada/tagueada. Só recebe
  commit quando uma versão fecha de verdade (release, ou patch
  urgente — ver abaixo).
- `nightly` = todo trabalho do dia a dia. Push livre, build/selftest
  podem ficar quebrados temporariamente entre pushes — mas isso é
  último recurso, não desculpa padrão: bug encontrado durante o
  trabalho deve ser corrigido na hora como comportamento default;
  deixar quebrado só quando o tempo realmente acabou.
- Cadência de push pra `nightly`: ao fim de todo item concluído, E ao
  fim de toda sessão de trabalho independente de ter terminado o
  item ou não — nunca encerrar uma sessão sem push pra `nightly`.
- Bug crítico encontrado no meio de trabalho grande: corrigir direto
  em `main` (não em `nightly`), tagueado como patch (ex: v1.0.1),
  depois merge dessa correção de volta pra `nightly` antes de
  continuar o trabalho.
- Quando o trabalho em `nightly` estiver feature-complete pra fechar
  uma versão: polish final (README, CHANGELOG, version.h, ROADMAP em
  dia) → `selftest` de verdade passando → push pra `nightly` → merge
  `nightly` → `main`.
- Cada release de `main` também vira uma entrada extra e permanente
  no menu de boot do GRUB, como kernel "anterior" — ver seção de Safe
  Mode abaixo pra motivo.

## Convenções de fim de fase (versionamento)

- kernel/version.h é a ÚNICA fonte de verdade pro número de versão
  do projeto (`NULLOS_VERSION`, `NULLOS_PHASE`, `NULLOS_PHASE_DESC`,
  e as strings compostas `NULLOS_BANNER`/`NULLOS_SHORT_BANNER`).
- Esquema `MAJOR.MINOR.PATCH` (a partir da v0.14.1): MINOR é
  reservado EXCLUSIVAMENTE pro número de fase concluída — nunca pule,
  nunca invente um MINOR que não corresponda a uma fase real
  concluída e documentada na tabela de fases concluídas do README
  (`NULLOS_PHASE` segue o mesmo número). PATCH é pra trabalho
  intermediário que NÃO constitui uma fase nova — reorganização de
  documentação, ferramentas de teste, pequenas funções aditivas que
  não mudam comportamento visível do usuário, correções de
  documentação — e NÃO toca em MINOR/`NULLOS_PHASE`/
  `NULLOS_PHASE_DESC`.
- **Sufixo `-nightly` (substitui a ideia antiga, já descartada, de
  sufixo de letra `-a`/`-b`/`-c` por subfase):** enquanto uma versão
  está sendo trabalhada em `nightly`, o número em version.h é
  `X.Y.Z-nightly`, onde `X.Y.Z` é a PRÓXIMA versão sendo construída
  (ex: depois de lançar v0.16.0, o trabalho em andamento é
  `0.17.0-nightly`). Ao fechar e mergear pra `main`, o sufixo cai e
  vira `X.Y.Z` limpo. NUNCA use letra de subfase no número de versão
  — divisão em subfase (18-A, 18-B, etc.) é rastreada só por texto de
  commit/CHANGELOG e pela tabela granular do ROADMAP.md (ver seção de
  Documentação acima), nunca no version.h.
- **Fluxo "Unreleased" dentro do CHANGELOG:** trabalho intermediário
  pequeno entra em CHANGELOG.md sob uma seção `## [Unreleased]` no
  topo do arquivo (seguindo as subsecções padrão do Keep a Changelog:
  Added/Changed/Fixed/Security etc.), SEM tocar em kernel/version.h.
  Isso pode se acumular ao longo de várias tarefas e vários pushes de
  `nightly` — não é preciso "fechar" a cada uma. `kernel/version.h`
  só é atualizado quando o usuário pedir explicitamente pra "fechar"
  essa versão. Fechar uma versão significa: renomear a seção
  `## [Unreleased]` do CHANGELOG pra `## [MAJOR.MINOR.PATCH] -
  <resumo>` (uma entrada por versão fechada, cobrindo TODAS as
  subfases que entraram nela — não uma entrada por subfase), remover
  o sufixo `-nightly` de version.h, e só então mergear `nightly` →
  `main`. Daqui pra frente, NUNCA faça bump de `kernel/version.h`
  automaticamente ao fim de uma tarefa pequena — espere o pedido
  explícito de "fechar" a versão. Isso não muda a regra de fase: uma
  fase concluída (bump de MINOR) ainda dispara o checklist completo
  de fim de fase abaixo, incluindo o bump de version.h no mesmo
  commit.
- NENHUM outro arquivo deve ter string de versão hardcoded a partir
  de agora — nem kernel/main.c (usa `NULLOS_BANNER` de version.h),
  nem user/shell.c (usa `NULLOS_SHORT_BANNER`, incluído via `-I` no
  Makefile de user/, já que version.h só tem macros de texto, sem
  tipo/função de kernel — seguro de incluir em código de userland),
  nem tools/grub.cfg (gerado em build-time a partir de
  tools/grub.cfg.in + kernel/version.h pela regra "GEN grub.cfg" no
  Makefile — nunca edite build/grub.cfg diretamente, é sobrescrito a
  cada build; o script de geração também decide, nesse mesmo passo,
  se a entrada de menu "GUI debug" entra ou não — ver seção de Safe
  Mode abaixo). Se descobrir uma string de versão hardcoded em
  qualquer lugar novo, ela é um bug de duplicação e deve ser
  substituída por uma referência a version.h (ou, se for um arquivo
  que não é C, gerada em build-time a partir dele), não corrigida
  manualmente toda vez que a versão mudar.
- Antes de finalizar qualquer fase, cumpra a "Definition of Done" (B) do início
  deste arquivo E faça este checklist explícito:
  kernel/version.h atualizado (sufixo `-nightly` removido)? README
  (tabela de fases concluídas) atualizado? docs/<assunto>.md
  relevante atualizado com o detalhe da feature? ROADMAP.md com a
  fase removida/movida se estava planejada lá, e a tabela granular
  atualizada com as subfases dessa versão? CHANGELOG atualizado (uma
  entrada só, cobrindo todas as subfases)? PROGRESS.md atualizado (se
  aplicável)? syscall.h conferido contra a tabela de syscalls de
  docs/syscalls.md (ver regra abaixo)? PROGRESS.md conferido como
  fonte primária do número de fase (ver regra abaixo)? bloco de
  banner ASCII hardcoded no README.md (o bloco de código logo no
  topo, com "NullOS vX.Y.Z - Phase N: ...") conferido e atualizado
  pra versão nova — ele NÃO é gerado automaticamente a partir de
  version.h como o banner de boot real ou o grub.cfg, precisa ser
  editado manualmente toda vez (isso já causou o mesmo problema mais
  de uma vez)? merge de `nightly` pra `main` feito? Só considere a
  fase "concluída" quando todos esses pontos estiverem sincronizados
  no mesmo commit.
- **Tag de release (convenção formal, sem precisar perguntar caso a
  caso):** toda versão fechada e mergeada em `main` recebe uma tag
  anotada `vX.Y.Z` (`git tag -a`, mensagem curta com versão e nome da
  fase) no mesmo momento do merge, apontando pro commit de fechamento em
  `main`, e a tag é enviada pro `origin`.
- **Snapshot da release anterior (`tools/prev/`):** logo depois de criar
  a tag, e ANTES de começar a próxima fase, rodar `make snapshot` NA
  ÁRVORE DA TAG (um worktree ou checkout limpo da tag, com `make clean &&
  make` antes — nunca na árvore de trabalho de `nightly`) e commitar o
  `tools/prev/` resultante de volta em `nightly`. Assim a entrada
  "previous release" do GRUB da versão seguinte é de fato a release
  anterior e não um build da própria versão em andamento (isso já
  aconteceu no fechamento da 0.18.0: `tools/prev/` tinha um nightly de
  si mesmo e teve que ser refeito a partir da tag v0.17.1). Se o alvo
  `snapshot` ainda não existir na tag (releases anteriores à 0.18.0),
  copiar `build/nullos.elf` e `build/ramfs.img` pro mesmo layout à mão.
- NUNCA invente um número de versão pra uma fase que não existe ou
  não foi pedida — se não tiver certeza do número de fase correto,
  pergunte antes de decidir, não assuma.
- **Filosofia de MAJOR (a partir do dia em que a v1.0.0 existir):**
  MAJOR (ex: 2.0) é reservado EXCLUSIVAMENTE pra quebra de
  compatibilidade real da interface pública (syscall removida ou com
  comportamento incompatível, formato de disco incompatível) — nunca
  por "marco grande" (suporte a nova arquitetura, GUI, rede, package
  manager — tudo isso é aditivo, então é MINOR, por maior que seja o
  esforço de implementação).

## Subfases dentro de uma fase

- Uma fase vira candidata a subdivisão em letra (18-A, 18-B, ...)
  quando acumula itens heterogêneos demais pra fechar de uma vez só
  em `nightly` sem virar um push gigantesco e arriscado.
- Ao dividir, cada subfase deve ser da MESMA espécie de trabalho que
  as outras dentro da mesma fase (ex: "consertar coisa que já existe
  e está errada" é uma espécie; "construir capacidade nova que ainda
  não existia" é outra). Se duas subfases têm critério de "pronto"
  fundamentalmente diferente (uma só roda `selftest`, outra exige
  teste manual de UI, por exemplo) ou uma depende estruturalmente da
  outra pra fazer sentido, considere separar em FASES diferentes em
  vez de subfases da mesma fase — subfase é pra variação de escopo
  dentro da mesma espécie de trabalho, não pra empilhar categorias
  diferentes só porque surgiram juntas na conversa.
- Cada subfase pode virar um ou mais pushes de `nightly` (build/teste
  podem ficar quebrados entre pushes dela). Só quando TODAS as
  subfases da fase estiverem prontas E testadas juntas é que a fase
  fecha como uma versão única (ver seção de versionamento acima).

## Regra de push

- REGRA DE PUSH: só recomendar/fazer `git push` (pra `nightly`, ver
  seção de branch acima) quando pelo menos um arquivo de código
  (`.c`, `.h`, `.asm`) tiver sido modificado nesta tarefa — mesmo que
  a mudança seja não-funcional (só comentário, só formatação,
  refactor sem mudança de comportamento). Mudança de código sempre
  libera push. Além disso, nunca encerrar uma sessão de trabalho sem
  fazer esse push, independente do item ter fechado ou não (regra de
  cadência da seção de branch).
- EXCEÇÃO: documentação pura (`README.md`, `ROADMAP.md`,
  `CHANGELOG.md`, `docs/*.md`, `PROGRESS.md`) sozinha NÃO libera push
  — o trabalho fica commitado localmente mas não é empurrado pro
  remoto — A MENOS QUE a mudança de documentação seja para corrigir
  um esquecimento de uma versão de código JÁ PUSHADA anteriormente
  (por exemplo: descobrir que o README nunca documentou uma syscall
  que já existe no código desde uma versão anterior, e corrigir isso
  agora). Nesse caso específico, a correção de documentação "atualiza
  retroativamente" uma versão já lançada e portanto libera push mesmo
  sem mudança de código nesta tarefa.
- Quando incerto se uma mudança de documentação se qualifica para a
  exceção, pergunte ao usuário antes de sugerir push, em vez de
  assumir.
- Push pra `main` só acontece no momento de fechar/mergear uma
  versão, ou pra um patch crítico urgente (ver seção de branch) —
  nunca como parte do fluxo comum de tarefa pequena.

## Convenções de fim de fase (números de syscall)

- kernel/syscall.h é a ÚNICA fonte de verdade pros números de
  syscall (os `#define SYS_*`). Sempre que a tabela de syscalls de
  docs/syscalls.md for escrita ou atualizada, rode um `grep` em
  syscall.h e confira CADA número da tabela contra o valor real do
  `#define` correspondente antes de escrever — nunca reafirme um
  número "de memória" (do que foi discutido na conversa) nem copie
  de uma versão anterior do arquivo sem checar, mesmo que pareça
  óbvio que não mudou.
- Ao concluir qualquer fase que adicione, remova, ou renumere uma
  syscall, o checklist de fim de fase (ver seção de versionamento
  acima) DEVE incluir explicitamente: "grep no syscall.h pra
  confirmar que os números atuais batem com o que docs/syscalls.md
  documenta" — isso é tão obrigatório quanto atualizar o banner de
  versão, não uma checagem opcional.
- Se o grep encontrar qualquer divergência (número que mudou, syscall
  nova sem entrada na tabela, ou entrada na tabela sem `#define`
  correspondente), corrija docs/syscalls.md imediatamente como parte
  da mesma tarefa — não deixe a divergência documentada "pra depois".

## Convenções de fim de fase (número de fase / roadmap)

- PROGRESS.md é a fonte PRIMÁRIA de qual foi a última fase (e
  subfase) concluída — não README.md, não CHANGELOG.md. Isso é
  deliberado: PROGRESS.md é o arquivo de contexto lido no início de
  toda sessão nova (ver seção "Memória de trabalho" acima), então se
  ele estiver certo, uma sessão futura já começa com a informação
  correta mesmo que não tenha lido README/CHANGELOG ainda.
- Sempre que precisar confirmar ou atualizar "qual é a fase atual" —
  seja pra decidir o próximo número de fase, seja pra revisar se o
  banner/roadmap/changelog estão em dia — confira PRIMEIRO o que
  PROGRESS.md → "Current status" diz, e trate README.md/CHANGELOG.md
  como precisando ser conferidos CONTRA esse valor, nunca o
  contrário. Se os três divergirem, PROGRESS.md é o desempate
  (e a divergência em si já é um bug de sincronização a ser
  corrigido nos outros dois).
- Isso existe porque já aconteceu nesta sessão: o banner de versão
  ficou 4 fases atrasado (preso em "Fase 10" com as Fases 11-13 já
  concluídas) sem que ninguém percebesse até uma auditoria explícita
  pedir a conferência. Uma sessão que confia cegamente no README/
  CHANGELOG sem checar contra PROGRESS.md pode repetir esse mesmo
  erro silenciosamente.

## Registro obrigatório no CHANGELOG

(Esta seção detalha o item de CHANGELOG da "Definition of Done", A e B, no
início deste arquivo: a entrada não pode ser adiada para o polish, e a fase não
fecha sem a entrada consolidada existir no arquivo.)

- TODA tarefa concluída nesta sessão — mesmo que seja só documentação
  pura, mesmo que não libere `git push` pela regra de push já
  estabelecida — deve ganhar uma entrada em `CHANGELOG.md` sob
  `[Unreleased]`, na subseção apropriada (Added/Changed/Fixed/etc).
- Isso vale inclusive para tarefas pequenas de reorganização,
  correção de texto, ou ajuste de arquivo `.md` que não mudam
  comportamento nenhum do sistema — o objetivo não é rastrear
  "mudança de comportamento", é rastrear "o que foi feito nesta
  sessão", pra sessões futuras (e o próprio usuário) terem histórico
  completo do que já foi tocado, sem precisar vasculhar o histórico
  de conversa pra descobrir.
- Só é aceitável pular a entrada de CHANGELOG quando a tarefa for
  puramente exploratória/de leitura (ex: "leia esse arquivo e me diga
  o que acha", sem nenhuma edição de arquivo real) — qualquer tarefa
  que resulte em edição de algum arquivo do repositório precisa de
  entrada correspondente no CHANGELOG.
- Ao fechar uma versão, `[Unreleased]` vira UMA ÚNICA entrada
  `## [X.Y.Z]` — não crie uma entrada por subfase (18-A, 18-B, ...)
  dentro do CHANGELOG; a granularidade de subfase mora na tabela do
  ROADMAP.md, não aqui.

## Configuração do sistema (arquivo chave=valor)

- Formato: texto simples, uma linha por entrada, `chave=valor` (ex:
  `boot_fail_count=0`). Sem parser sofisticado, sem formato
  estruturado (JSON/XML) — separar por `=` é suficiente.
- Um arquivo só, reaproveitado por tudo que precisar persistir estado
  simples entre boots (contador de falha de boot, layout de teclado,
  cor de fundo da GUI quando existir, modo de texto padrão, etc.) —
  não criar um arquivo de config por feature.
- MANTER PEQUENO DE PROPÓSITO: o arquivo deve caber inteiro num único
  setor de disco. Isso não é só economia — é a garantia de
  atomicidade de escrita (um setor escreve inteiro ou não escreve
  nada, sem estado "meio corrompido" por queda de energia no meio do
  caminho). Se um dia esse arquivo crescer a ponto de precisar de
  mais de um setor, ISSO é o sinal de que chegou a hora de resolver
  escrita atômica de verdade (arquivo temporário + rename) — não
  adicione essa complexidade antes disso ser um problema real.
- Lido bem cedo no `kernel_main()`, direto via chamada de função de
  FAT16 — nunca via `exec`/processo (mesma restrição do Safe Mode).

## Safe Mode

- **Onde vive:** dentro do MESMO binário de kernel, não um kernel
  separado nem um processo de usuário. Bem cedo em `kernel_main()`
  — depois só do mínimo indispensável (PS/2, VGA texto, ATA PIO) —
  uma checagem de flag de boot (vinda do parâmetro de linha de
  comando do Multiboot, no x86; a função que lê essa flag é o único
  ponto que precisa reimplementação por arquitetura no futuro, ex:
  argumento `bootargs` de Device Tree no ARM) decide se desvia pro
  loop de monitor do Safe Mode. Safe Mode roda inteiro em ring 0,
  ANTES de scheduler, `process_spawn_user`, troca pra modo usuário, ou
  `syscall.c`/`int 0x80` serem inicializados — nunca depois. Isso é
  deliberado: o propósito do Safe Mode é sobreviver a bug justamente
  nesses subsistemas, então ele não pode depender deles. Essa
  arquitetura (branch cedo dentro do mesmo binário, em vez de um
  segundo binário carregado via módulo Multiboot separado) também é
  a mais portável pra um porte futuro de arquitetura, já que não fica
  amarrada ao protocolo de boot específico do x86/GRUB.
- **Gatilho automático:** contador `boot_fail_count` no arquivo de
  config (ver seção acima), incrementado no início do boot e zerado
  só quando o sistema confirma ter chegado num estado considerado "deu
  certo" (shell ou GUI carregou sem travar). Depois de N falhas
  seguidas (valor a definir na implementação), entra em Safe Mode
  sozinho.
- **TUI:** menu numerado com submenus pra ação que precisa de mais de
  uma etapa — ação destrutiva (apagar arquivo) SEMPRE passa por
  confirmação explícita numa tela própria, nunca direto no menu
  principal; operação com modo "só verificar" vs "verificar e
  reparar" (fsck) também vira submenu, nunca decisão implícita.
- **Shell restrito:** comandos EMBUTIDOS apenas — cada um chama
  direto a função de baixo nível (FAT16, VGA, PS/2) como chamada de
  função C comum, NUNCA via `process_spawn_user`/`exec`. Isso vale mesmo
  pra comando com nome igual a um programa userland existente (ex:
  `cat`) — se `cat` normal é um binário separado lançado via `exec`,
  o `cat` do Safe Mode é uma reimplementação mínima própria, não uma
  chamada pro `cat.elf` de sempre. Sem comando `run` (ou qualquer
  forma de lançar processo externo). `help` é uma string ESTÁTICA
  (não gerada dinamicamente a partir de tabela de dispatch) — o
  conjunto de comando do Safe Mode é pequeno e deliberadamente
  congelado, então o custo de manter a string em sincronia é baixo;
  ao adicionar/remover um comando do Safe Mode, atualizar a string de
  `help` manualmente é parte obrigatória da mesma tarefa.
- **Menu do GRUB:** 4 entradas — Padrão (GUI), GUI debug (GUI com um
  terminal universal já aberto, que é o "console do sistema" e
  espelha pra serial — só existe em build `nightly`, decidido em
  build-time pelo mesmo script que já gera `tools/grub.cfg`, nunca
  detectado em runtime), Text mode (o modo texto VGA atual), e Safe
  Mode. Reiniciar em GUI debug também é uma opção dentro do submenu
  de "Reiniciar" do Safe Mode, sempre disponível ali independente da
  entrada principal do GRUB estar ou não visível.
- **Fallback além do Safe Mode:** toda vez que `nightly` for
  mergeado em `main` (fechamento de versão), o binário de kernel
  anterior de `main` deve ser preservado e virar uma entrada extra e
  permanente no GRUB ("NullOS vX.Y.Z (anterior)") — isso cobre o
  cenário que o Safe Mode sozinho não cobre (bug no próprio código de
  boot/paginação/GDT/IDT, que roda antes de qualquer flag de Safe
  Mode ser lida).