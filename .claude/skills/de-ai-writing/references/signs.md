Source: Wikipedia, Signs of AI writing (WikiProject AI Cleanup), September 2026 edition. The guide is a field guide for spotting machine-written text. Here it is turned around: each sign comes with what it looks like, the words to watch, and how a human writer would fix it. Signs that only apply to Wikipedia markup (wikitext, citation templates, categories, edit summaries) are left out.

The numbering matches scripts/check_ai_signs.py.

Contents
Content signs (1.1–1.8)
Language and grammar (2.1–2.6)
Style and formatting (3.1–3.8)
Text meant for the chat user (4.1–4.3)
Older-model habits (5.1–5.2)
What human writing does instead
Not signs: don't "fix" these

1. Content

1.1 Undue emphasis on significance, legacy and broader trends
Ordinary facts get a sentence about why they matter, how they "reflect broader" trends, or their "enduring legacy". Even mundane subjects (a railway station, a fish species) get puffed up.

Words to watch: stands/serves as, is a testament/reminder, a crucial/pivotal/vital/significant/key role or moment, underscores/highlights its importance, reflects broader, symbolising its ongoing/enduring/lasting, contributing to the, setting the stage for, marking/shaping the, represents/marks a shift, key turning point, evolving landscape, focal point, indelible mark, deeply rooted, "has generated debate about…", "prompted broader reflection on…".

Fix: delete the significance sentence, or replace it with a specific fact that shows the point.

Before: "The institute was established in 1989, marking a pivotal moment in the evolution of regional statistics." After: "The institute was set up in 1989. It was Spain's first regional statistics office."

1.2 Canned emphasis on notability and media coverage
Proving importance by listing where something was covered, or describing the sources themselves.

Words to watch: independent coverage, local/national media outlets, trade publications, cited/featured/profiled in, written by a leading expert, maintains an active social media presence.

Fix: say what the thing is or did. Cite coverage only as a source for a specific claim.

1.3 Superficial analysis
A participle clause tacked onto the end of a factual sentence that interprets it without adding information: "…, highlighting its importance", "…, ensuring…", "…, reflecting…".

Words to watch: highlighting, underscoring, emphasising, ensuring, reflecting, symbolising, contributing to, cultivating, fostering, encompassing, enhancing, showcasing, valuable insights, align/resonate with.

Fix: cut the clause. If the interpretation matters, make it its own sentence with evidence.

Before: "The station has 8 tracks and 6 platforms, reflecting its continued relevance in the regional transportation landscape." After: "The station has 8 tracks and 6 platforms."

1.4 Promotional, advert or travel-brochure language
Neutral subjects written up like a hotel listing or a press release.

Words to watch: boasts a, vibrant, rich (culture/heritage/history), profound, enhancing, showcasing, exemplifies, commitment to, natural beauty, nestled, in the heart of, groundbreaking, renowned, featuring, diverse array, breathtaking, stunning, world-class, seamless, state-of-the-art.

Fix: replace the adjective with the fact that would earn it, or drop it.

Before: "Nestled in the heart of the old town, the café boasts a diverse array of pastries." After: "The café is on the main square and sells about twenty kinds of pastry."

1.5 Vague attributions and overgeneralised opinions
Opinions hung on unnamed authorities, or one source presented as many.

Words to watch: industry reports, observers have cited, experts argue, some critics argue, several sources (when there are one or two), studies show, widely regarded, "such as" before what is actually a complete list.

Fix: name the source, or state the claim as your own view, or cut it.

1.6 Formulaic "challenges and future prospects" endings
"Despite its [praise], X faces several challenges… Despite these challenges, X continues to thrive…", often in a closing "Challenges" or "Future outlook" section.

Fix: if a problem is real, state it plainly where it belongs. Drop the optimistic wrap-up.

1.7 Leads that define the title as a thing
"X refers to…" or "The List of Y is a curated compilation of…" for topics or lists.

Fix: start with the subject itself. "A catchment area is the area a hospital draws patients from."

1.8 "X and Y" headings, "Awards and recognition"
Paired headings built to sound complete. Keep a heading only if the section needs it.

2. Language and grammar

2.1 High density of "AI vocabulary"
One or two of these is normal. Several in one piece of text is one of the strongest tells.

2023–mid-2024 set: additionally (especially opening a sentence), boasts, bolstered, crucial, delve, emphasising, enduring, garner, intricate/intricacies, interplay, key (adjective), landscape (abstract), meticulous, pivotal, underscore, tapestry, testament, valuable, vibrant. Mid-2024–2025 set: align with, bolstered, crucial, emphasising, enhance, enduring, fostering, highlighting, pivotal, showcasing, underscore, vibrant. Newer models: emphasising, enhance, highlighting, showcasing.

Fix: use the ordinary word or cut. "Additionally" → start the sentence without it, or "Also". "Crucial" → "important", or explain why. "Landscape" → name the actual thing. Don't just swap in a synonym: the guide warns that the word is a symptom; the padding is the problem.

2.2 Avoiding "is", "are" and "has"
Dressing up a plain verb.

Words to watch: serves as, stands as, marks, functions as, operates as, represents, boasts / features / maintains / offers (meaning "has"), refers to, "ventured into politics as a candidate" (was a candidate), "began his career as" (was).

Fix: use is/are/has. "Gallery 825 serves as LAAA's exhibition space. The gallery features four separate spaces" → "Gallery 825 is LAAA's exhibition space. It has four galleries."

2.3 Vague connection or association
"was associated with leadership of", "in connection with", "connected to" where a plain relationship is known. Fix: state the relationship. "In 2017 she was CEO of ExampleCorp."

2.4 Negative parallelisms
Correcting a misconception nobody raised.

"Not only … but also …", "It's not just X, it's Y", "This isn't A, it's B"
"Not X, but Y", "X is not a mirror but a portal"
"No X, no Y, just Z"
"Y rather than X" (overused; fine occasionally)
Fix: state the positive claim alone. "It's not just a coffee shop, it's a gathering place" → "People stay for hours." Keep a contrast only when a reader really would assume the opposite.

2.5 Rule of three
Triplets of adjectives or short phrases used to sound thorough: "fast, reliable, and secure"; "learn, connect, and grow". Fix: keep lists that are really lists of facts; otherwise pick the one item that matters, or two. Vary rhythm.

2.6 Elegant variation
Cycling through synonyms to avoid repeating a noun ("the artist… the painter… the creative… the visionary"). Fix: repeat the plain noun, or use a pronoun.

3. Style and formatting

3.1 Title Case headings
"Impact Of Technology And Digitalization". Fix: sentence case, unless house style says otherwise.

3.2 Overuse of boldface
Bolding key phrases throughout, "key takeaways" style. Fix: bold nothing in body prose, or at most a term at its definition.

3.3 Inline-header vertical lists
Bullets that start with a bold label and a colon: "Scalability: The system…". Fix: turn into prose, or a plain list without labels, or a real table if the data is tabular.

3.4 Overuse of em dashes
LLMs use em dashes where people use commas, colons, parentheses or full stops, often spaced " — " for punch. Claude in particular uses more than professional writers do. Fix: rewrite the sentence with ordinary punctuation. Don't swap in an en dash or hyphen; that keeps the pattern. One or two in a long piece is fine.

3.5 Emoji as formatting
Emoji in front of headings or bullets. Fix: remove.

3.6 Unusual small tables
Two- or three-row tables that would read better as one sentence.

3.7 Curly quotes and apostrophes
Mixed curly (" " ') and straight quotes. Match the surrounding text's convention. (Word processors also produce curly quotes, so this alone proves nothing.)

3.8 Thematic breaks between every section
--- between every section. Remove unless the format needs them.

4. Text meant for the chat user

4.1 Collaborative communication left in
"I hope this helps", "Certainly!", "Great question", "Let me know if…", "Would you like me to…", "Here is a…", "In this section, we will discuss…". Fix: delete.

4.2 Knowledge-cutoff disclaimers and speculation about gaps
"As of my last update…", "While specific details are limited…", "not widely documented/disclosed", "based on available information", "maintains a low profile", followed by guesses about what the missing information "likely" is. Fix: if something genuinely is unknown, say so once, concretely, and tell the reader what to do: "Opening hours aren't posted; call ahead." Never pad the gap with speculation.

4.3 Placeholders
"[Your Name]", "[Describe the section]", "INSERT_URL", "2025-XX-XX". Fix: fill in or remove.

5. Older-model habits (still worth avoiding)

5.1 Didactic disclaimers
"It's important to note/remember/consider", "It is worth noting", "may vary", "keep in mind". Fix: state the thing. "Hours may vary" → "Hours change in winter."

5.2 Section summaries
Paragraphs that end by restating themselves; openers like "In summary", "In conclusion", "Overall", "Ultimately". Fix: end on the last new fact.

6. What human writing does instead

From the guide's "Signs of human writing":

Plain is/has sentences: "there is a", "it has a".
Plain words: wrote (not authored), moved (not relocated), used (not utilised), tried (not attempted), died (not passed away), before (not prior to), to (not in order to).
Definite statements where true: "is the only", "was the first", "one of the best".
Ordinary hedges and intensifiers: "very", "perhaps", "tends to".
Specific, unusual facts. AI text regresses to the generic ("a revolutionary titan of industry"); human text keeps the odd detail ("inventor of the first train-coupling device").

7. Not signs: don't "fix" these

The guide lists these as ineffective indicators. Changing them wastes effort and can make text worse:

Perfect grammar.
Mixing casual and formal registers.
"Bland" or formal prose in general, or long words in general.
A single transition word on its own.
Unsourced content.
Correct formatting. Quoted material (a real person's words, a title, a source) is never rewritten to remove signs.
