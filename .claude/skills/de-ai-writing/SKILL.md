---
name: de-ai-writing
description: Rewrite text so it has no signs of AI writing, using Wikipedia's "Signs of AI writing" field guide (em-dash overuse, "not just X, it's Y", "serves as", "delve/crucial/pivotal/tapestry", puffed-up significance, promotional tone, vague attributions, "it's important to note", "In conclusion", Title Case headings, bold-label bullet lists, chatbot leftovers). Use this whenever someone wants content de-AI'd, humanised, made to "sound less like ChatGPT/AI", "less robotic", cleaned of AI tells, or checked for signs of AI writing before publishing, including blog posts, web copy, FAQs, emails, essays, LinkedIn posts, product pages, docs and whole websites or repos of content. Also use it when polishing a draft the user says came from an AI.
---

# De-AI writing

Rewrite content so it reads as written by a knowledgeable person, by removing the patterns catalogued
in Wikipedia's *Signs of AI writing* (WikiProject AI Cleanup). The full catalogue, with words to
watch and a before/after fix for each sign, is in `references/signs.md`. Read it before the first
rewrite in a session; it is the source of truth for what counts as a sign.

## Why this works the way it does

The Wikipedia guide makes a point worth holding on to: the signs are symptoms. Language models
regress to the most statistically common phrasing, so specific facts get smoothed into generic,
important-sounding statements. Swapping a flagged word for a synonym, or an em dash for an en dash,
hides the symptom and keeps the problem. The aim is text that says specific things plainly. When you
fix a sentence, fix what it is doing (puffing up, hedging, correcting a misconception nobody raised),
not just the word that tripped the scanner.

## Workflow

1. **Get the text.** Pasted text, a file, a folder of files, or a built website. For a codebase or
   site, find where the copy actually lives (data files, CMS exports, templates) and edit there, not
   in generated output.

2. **Scan it.** If you can run code, use the bundled scanner for a baseline:
   ```bash
   python3 scripts/check_ai_signs.py <file(s)>      # .txt, .md or .html; "-" reads stdin
   python3 scripts/check_ai_signs.py <file> --summary
   ```
   It reports each matching sentence under its sign number. It catches pattern-matchable signs only.
   Rule of three, elegant variation, puffed-up structure and summary endings need your own read.
   Without code execution, do the same pass by hand using `references/signs.md`.

3. **Rewrite.** Go sentence by sentence through everything flagged, then read the whole piece once
   more for the judgement signs. Rules for the rewrite:
   - **Keep every fact, number, name, date, link and claim.** De-AI'ing changes how things are said,
     never what is said. If a sentence was only padding, delete it; if it carried a fact, keep the fact.
   - **Keep the author's voice and conventions**: person (I/we/you), spelling (UK/US), tone,
     formatting system (Markdown, HTML, JSX, plain text), and any length limits (SEO titles ~60
     characters, meta descriptions ~155, button labels).
   - **Never alter quotations** of real people or sources, titles of works, or legal text.
   - **Em dashes**: rewrite with commas, colons, parentheses or full stops. Don't substitute another
     dash. One or two in a long piece is fine.
   - **Contrasts** ("not X but Y", "rather than", "not just… it's"): state the positive claim on its
     own unless a reader genuinely would assume the opposite.
   - **Unknowns**: if the original says something isn't known, keep that honesty, but say it once,
     concretely, with what the reader should do ("Hours aren't posted; call ahead"). Don't pad it
     with speculation.
   - **Plain words**: is/has instead of serves as/boasts; used, tried, moved, to, before.
   - **Don't over-correct.** Perfect grammar, formal register, a single transition word or an
     unsourced claim are *not* signs (see section 7 of the reference). Leave clean sentences alone.

4. **Re-scan and re-read.** Run the scanner again and aim for zero hits. A hit you keep on purpose
   (a quotation, a proper noun like "Key Largo", a word used literally, such as "landscape" for a
   painting) is fine; note it in your report.

5. **Report briefly.** Give the rewritten text (or confirm files were edited), then a short note:
   sign counts before → after, two or three representative before/after examples, and anything
   left in deliberately. Keep the note short; the rewrite is the deliverable.

## Quick reference

The signs, in brief (details and fixes in `references/signs.md`):

| # | Sign | Typical tells |
|---|------|---------------|
| 1.1 | Undue significance / legacy | stands as, testament, pivotal role, reflects broader, enduring legacy |
| 1.2 | Canned notability | featured in, independent coverage, active social media presence |
| 1.3 | Superficial -ing analysis | "…, highlighting/ensuring/reflecting…", valuable insights |
| 1.4 | Promotional tone | vibrant, nestled, in the heart of, boasts, world-class, seamless |
| 1.5 | Vague attribution | experts say, observers note, studies show, widely regarded |
| 1.6 | Challenges formula | "Despite its…, faces challenges… Despite these challenges…" |
| 1.7 | Title-defining lead | "X refers to…" |
| 2.1 | AI vocabulary | additionally, crucial, delve, enhance, foster, key, landscape, robust, tapestry |
| 2.2 | Copula avoidance | serves as, functions as, represents, features/offers (for "has") |
| 2.3 | Vague association | associated with, in connection with |
| 2.4 | Negative parallelism | not only…but, it's not X it's Y, no X no Y just Z, rather than |
| 2.5 | Rule of three | stacked triplets used for rhythm |
| 2.6 | Elegant variation | synonym cycling for one noun |
| 3.1–3.8 | Formatting | Title Case headings, bold everywhere, **Label:** bullets, em dashes, emoji, tiny tables, mixed curly quotes, --- between sections |
| 4.1–4.3 | Chat leftovers | I hope this helps, let me know, "not widely documented", [Your Name] |
| 5.1–5.2 | Older habits | it's important to note, may vary, In conclusion, Overall |

## Example

**Input**
> Nestled in the heart of the city, our café stands as a testament to the town's rich cultural
> heritage — it's not just a coffee shop, it's a gathering place. Additionally, the menu boasts a
> diverse array of pastries, showcasing our commitment to quality. While specific details about
> opening hours are not widely documented, it's important to note that hours may vary.

**Output**
> Our café is on the main square, in a building that has been a bakery since 1911. People come for
> the coffee and stay for hours. We bake about twenty kinds of pastry each morning. Opening hours
> change with the seasons, so check our Instagram before you visit.

(The details "1911", "twenty kinds" and "Instagram" would come from the user. If the facts aren't
available, ask, or write the plain version without them: "Our café is on the main square. We bake
pastries every morning. Hours change with the seasons, so check before you visit." Never invent
facts to replace puffery.)
