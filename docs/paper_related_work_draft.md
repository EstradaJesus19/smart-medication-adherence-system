# Related Work draft

The LaTeX section below contains 5842 characters including spaces and source commands (LF line endings). The typeset text is approximately 5519 characters including spaces and headings, excluding the bibliography. It is a focused narrative comparison, not a systematic review.

Add paper_related_work_references.bib to the existing References.bib database. Retain the existing checchi2014packaging and choudhry2017remind entries.

```latex
\section{Related Work}
Medication-support technologies address complementary tasks: organizing medicines, prompting timely use, recording interactions, and controlling access to doses. The following studies illustrate their benefits, limitations, and relevance to a compartment-based organizer.

\subsection{Packaging and Reminder Aids}
Conventional pillboxes and blister packs organize medicines into predefined doses or administration times. Conn et al.\ synthesized 52 reports involving 22,858 participants and found an overall benefit of packaging interventions, with stronger effects for blister packs and pharmacy-delivered interventions \cite{conn2015packaging}. However, publication bias and sparse reporting of health outcomes limit interpretation. Packaging offers an organizational foundation whose effectiveness depends on the intervention and population.

Electronic reminders add a temporal cue to this organizational role. Vervloet et al.\ reviewed 13 studies of text messages, audiovisual devices, and pagers, finding evidence of short-term benefit but uncertainty about sustained effects \cite{vervloet2012reminders}. Conversely, the REMIND trial, involving 53,480 participants, found no significant adherence improvement from a standard pillbox, digital timer cap, or bottle-mounted tracking strip \cite{choudhry2017remind}. These results motivate connecting reminders to understandable actions and evaluating their usefulness during daily routines.

\subsection{Mobile Applications}
Smartphone applications extend reminders with configurable schedules, reports, and optional social support. In the MedISAFE-BP trial, Morawski et al.\ randomized 412 adults with uncontrolled hypertension to an application or control condition \cite{morawski2018medisafe}. At 12 weeks, the intervention produced a small improvement in self-reported adherence without a significant difference in systolic blood pressure. Applications offer flexible interfaces but require separate assessment of engagement and clinical outcomes. They can complement an organizer without replacing physical compartment guidance.

\subsection{Instrumented Pill Organizers}
Instrumented organizers combine physical storage with automatic interaction records. Hayes et al.\ developed MedTracker, a seven-day pillbox that records compartment openings, and examined its use in a five-week vitamin-regimen study involving 39 older adults \cite{hayes2006pillbox}. This approach illustrates how a familiar object can collect detailed timing information without requiring users to enter every event manually.

Karagiannis et al.\ presented a portable connected pillbox combining reminders and monitoring \cite{karagiannis2022pillbox}. In a 14-person simulated-regimen evaluation, recorded delays were shorter than with a dummy pillbox, but wrong or missed pill errors did not differ significantly. This small study illustrates the value of evaluating timing and errors separately, without establishing clinical effectiveness.

At a broader level, Checchi et al.\ reviewed 37 studies of electronic medication packaging and identified event recording, audiovisual reminders, displays, real-time monitoring, and feedback as common features \cite{checchi2014packaging}. Evidence varied in quality, and devices integrated with care delivery were more frequently associated with improved adherence. These findings shift attention from individual features to how monitoring, feedback, and care processes work together in a specific setting.

\subsection{Dispensing and Connected Care}
Automated dispensers take a further step by controlling access to prepared medication doses. Faisal et al.\ identified 114 smart adherence products across 15 categories, including dispensers, medication trays, blister packages, and electronic caps \cite{faisal2023features}. These categories offer different levels of assistance, with substantial variation in capacity, alarms, locking, connectivity, and cost. Their review compared reported features rather than experimentally establishing which product was most effective. Dispensing is therefore a distinct design choice whose additional access control must be weighed against setup and maintenance requirements.

Connected systems also extend support beyond the device. Shahani et al.\ studied implementation of an electronic pillbox intervention combining prefilled trays, reminders, remote alerts, and provider-accessible reports \cite{shahani2022pillbox}. Reported barriers included portability, medication-data entry, and the coordination required around hospital discharge. Richer functionality therefore introduces service and workflow requirements alongside technical ones.

\subsection{Usability and Position of the Present Work}
Usability provides another basis for comparing these approaches. Patel et al.\ evaluated 21 electronic adherence products with 39 older adults, caregivers, and health care providers \cite{patel2020usability}. Across 186 product tests, all required steps were completed without assistance in only 55.3\% of cases, with substantial variation in workload and usability. This finding motivates evaluating setup, feedback, and routine handling alongside technical functions.

The present project focuses on a four-compartment organizer integrating reminders, compartment guidance, and interaction recording. This scope retains the organizational role of a pillbox while adding observable feedback, without the access-control mechanisms of a dispenser or the service infrastructure of connected clinical care. The contribution is a defined, testable integration of established functions, assessed through representative interactions. The literature motivates evaluation of functional consistency and user understanding; it does not justify assuming improved clinical adherence from device operation alone.
```
