# Project README template

Copy this into every `NN_*/README.md`. Sections are fixed so projects can be
compared and so the repo reads as one document. The write-up — especially
**Why the results happened** — is the part that matters in an interview.

---

```markdown
# Project NN — <Name>

Stage <N> · Concept: <the one thing this project teaches>

## 1. Objective
What question does this project answer? One or two sentences.

## 2. Environment
Machine, OS/kernel, compiler + flags, CPU model, core count, anything tuned
(governor, isolated cores, huge pages). Reference docs/ENVIRONMENT.md and note
only the deltas from it.

## 3. What I tested
The experiment(s), described so someone else could repeat them.

## 4. Baseline configuration
The unmodified/naive setup and why it is the right baseline.

## 5. <Changed> configuration
What was changed, and the hypothesis: *why should this be faster/more consistent?*
State the prediction before showing the numbers.

## 6. Measurements
How measurement was done: what was timed, iterations, warmup, repetitions,
what was excluded, how outliers were handled.

## 7. Results

| Metric | Baseline | Modified |
|---|---|---|
| ... | ... | ... |

Raw output: `results/<file>`

## 8. Why the results happened
The mechanism. Hardware/OS-level explanation, not "it was faster because it is
optimized". If the result contradicted the hypothesis in section 5, say so and
explain what was actually going on.

## 9. What I learned
Concepts genuinely understood, in your own words.

## 10. Limitations
What this does NOT prove. Measurement noise, WSL2 vs bare metal, single machine,
workload not representative, etc.

## 11. Next experiment
The follow-up question this raised, and which project answers it.

---

### Questions I can answer
- [ ] <the project's checklist of questions>
```
