# Preliminary paper draft

Draft date: 1 October 2026. The abstract and introduction describe the system design without development-stage details or claims of experimental results. Revise the abstract to incorporate the final findings when available. The section numbering assumes the structure in the professor's guide.

## Title

Smart Medication Adherence Support: Design and Simulation of a Four-Compartment Pill Organizer

## Abstract

Managing medication routines requires timely reminders and clear identification of the medication to access. This paper presents a four-compartment medication adherence support system designed around an ESP32-based pill organizer. The proposed architecture combines compartment-access monitoring with reminder management and structured event recording. A finite-state control model handles simultaneous dose occurrences, evaluates each compartment independently through a minimum continuous lid-opening interval, and distinguishes expected access from wrong-compartment and repeated-access events. Closure checks and a dedicated refill mode account for interactions beyond the reminder sequence. Hardware-independent control logic enables reproducible evaluation of timing and state transitions across desktop and embedded environments. The system is designed to support medication routines and provide interpretable records of compartment interactions. These records represent access behavior and do not establish pill removal or ingestion. The contribution is a testable embedded architecture with explicit rules for managing and interpreting compartment access.

## Keywords

Medication adherence support; electronic pill organizer; compartment-access monitoring; embedded systems; finite-state machine; software simulation.

## I. Introduction

Medication adherence concerns how patients take medicines in relation to the prescribed regimen. Vrijens et al. distinguish initiation, implementation, and discontinuation, separating the start of treatment from daily dosing behavior and the decision to stop [1]. A compartment-based organizer addresses the practical interaction between a user and stored medication: identifying the intended compartment, responding to a reminder, and accessing it within a defined time window. 

Electronic medication packaging provides a technical basis for this approach. Devices can combine event recording with audiovisual reminders, displays, and feedback. In a systematic review, Checchi et al. identified these features across evaluated systems, while emphasizing variability in study quality and limited evidence for their effectiveness [2]. Furthermore, the REMIND randomized trial found that three simple reminder devices did not significantly improve adherence compared with usual care in the population studied [3]. These findings motivate a careful distinction between delivering a reminder, observing a user interaction, and demonstrating a change in medication-taking behavior. They also support evaluating the engineering performance of a device separately from its effect on clinical adherence.

For a multi-compartment organizer, reminder management involves more than detecting whether a lid has opened. Several compartments may be associated with the same reminder time, and users may access them in different orders or simultaneously. Brief openings can differ from sustained access, while opening an unrelated compartment should not resolve a pending reminder. Reopening a compartment after its expected interaction has been confirmed requires a different interpretation from accessing another compartment. Refilling introduces a further distinction, since legitimate maintenance access should not trigger the same warnings as unexpected access during routine use. These cases call for explicit rules governing timing, state transitions, and event classification.

This paper presents a four-compartment Smart Medication Adherence System designed around an ESP32-based pill organizer. The proposed architecture combines lid-state inputs, reminder management, and structured event recording within a finite-state control model. Its objective is to guide compartment interactions and produce records whose meaning can be traced to defined behavioral rules. The design separates control logic from hardware inputs and outputs, allowing the same decision model to be evaluated in desktop and embedded environments. Local reminder operation is a design requirement, reducing dependence on external connectivity for the organizer's essential functions.

The control model handles one reminder session containing up to four compartment-specific dose occurrences. Each expected interaction is evaluated independently and confirmed when the corresponding lid remains continuously open for a configured minimum interval. Occurrences that do not satisfy this criterion within the reminder window remain unconfirmed. Wrong-compartment and repeated-access events generate distinct warnings without resolving other pending interactions. Closure checks ensure that open compartments remain part of the interaction sequence, while a dedicated refill mode distinguishes maintenance from reminder-related access. 

The scope of the system is compartment-access monitoring. A confirmed interaction indicates that the defined opening criterion has been satisfied; it does not demonstrate that medication has been removed or swallowed. Likewise, an unconfirmed interaction does not establish a clinically verified missed dose. The organizer supports reminders and observation without dispensing or counting pills. The main contributions are an explicit control model for concurrent compartment access, a hardware-independent embedded architecture, and a reproducible evaluation approach based on controlled interaction sequences and structured event records. 

The remainder of the paper is organized as follows. Section II reviews related medication-support and monitoring systems. Section III describes the proposed architecture, interaction rules, and implementation methodology. Section IV presents the experimental results and discusses their limitations. Section V concludes the work and outlines future developments.

## References

[1] B. Vrijens et al., "A new taxonomy for describing and defining adherence to medications," British Journal of Clinical Pharmacology, vol. 73, no. 5, pp. 691-705, 2012. DOI: [10.1111/j.1365-2125.2012.04167.x](https://doi.org/10.1111/j.1365-2125.2012.04167.x).

[2] K. D. Checchi, K. F. Huybrechts, J. Avorn, and A. S. Kesselheim, "Electronic medication packaging devices and medication adherence: A systematic review," JAMA, vol. 312, no. 12, pp. 1237-1247, 2014. DOI: [10.1001/jama.2014.10059](https://doi.org/10.1001/jama.2014.10059).

[3] N. K. Choudhry et al., "Effect of reminder devices on medication adherence: The REMIND randomized clinical trial," JAMA Internal Medicine, vol. 177, no. 5, pp. 624-631, 2017. DOI: [10.1001/jamainternmed.2016.9627](https://doi.org/10.1001/jamainternmed.2016.9627).

## Revision notes (outside the manuscript)

- The example paper informs the academic tone and broad-to-specific introduction structure; its technical claims are not evidence for this project.
- The professor's guide suggests approximately 100 characters for the title, 1,000-1,500 for the abstract, and 4,000-4,500 for the introduction, including spaces. This draft follows those approximate targets.
- The guide's recommendation of more than ten references concerns the complete paper. These three context references are a starting point; expand the bibliography with focused device research for Section II.
- The abstract contains no experimental result claims. Incorporate the principal findings after the final experiments.
- Development-stage details and the preliminary test count have been removed from the abstract and introduction.
