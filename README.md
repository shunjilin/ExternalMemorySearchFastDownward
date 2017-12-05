# External Memory Search with the Fast Downward Planner

Extended Fast Downward Planner for External Memory Optimal Search in
Domain Independent Planning.

This is the source code used in 'Revisiting Immediate Duplicate Detection in
External Memory Search' by Shunji Lin, Alex Fukunaga, accepted for the 32nd
AAAI Conference on Artificial Intelligence (AAAI-18).

This source code is built upon the [Fast
Downward](http://www.fast-downward.org/) Planner, [changeset:
10653:feead5e85396](http://hg.fast-downward.org/rev/9dd74e5e3951).

## External Memory Search Algorithms
+ A*-IDD (Lin, Fukunaga 2018)
+ A*-DDD (Korf 2004; Hatem 2014)
+ External A* (Edelkamp, Jabbar, and Schrodl 2004)

## Relevant Files/Subdirectories
Files relevant to external memory search can be found in src/search/external
Parameters for the external memory search algorithms can be found in their
specific files (see src/search/DownwardFiles.cmake) as well as in
src/search/search_engines/search_common*
(TODO: consolidate parameter tuning)

## Usage
Same syntax as fast-downward. Compile the external search (64-bit) build by running:
```
./build.py externalsearch64
```
Example of A*-IDD with merge and shrink:
```
./fast-downward.py --build=externalsearch64 ./downward-benchmarks/sokoban-opt11-strips/p01.pddl --search "astar_idd(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false), merge_strategy=merge_sccs(order_of_sccs=topological,merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order])), label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1))"
```
+ A*-DDD: astar_ddd  
+ External A*: external_astar
+ See src/search/DownwardFiles.cmake for available heuristics and to add any
path-independent heuristic

## Disclaimer
This has only been tested on a linux system.  
The use of mmap in A*-IDD requires a POSIX-compliant operating system.     
Requires boost/multiprecision/miller_rabin for primality testing in A*-IDD.

## References
+ Burns, E. A.; Hatem, M.; Leighton, M. J.; and Ruml, W. 2012. Implementing
fast heuristic search code. In Fifth Annual Symposium on
Combinatorial Search.
+ Edelkamp, S.; Jabbar, S.; and Schrödl, S. 2004. External A*. KI 4:226–240.
+ Hatem, M. 2014. Heuristic search with limited memory. Ph.D. Dissertation,
University of New Hampshire.
+ Korf, R. E. 1985. Depth-first iterative-deepening. Artificial Intelligence
27(1):97 – 109.
+ Lin, S.; Fukunaga, A. 2018. Revisiting Immediate Duplicate Detection in
External Memory Search. To appear in the 32nd
AAAI Conference on Artificial Intelligence (AAAI-18).

## Original Fast Downward README
Fast Downward is a domain-independent planning system.

For documentation and contact information see http://www.fast-downward.org/.

The following directories are not part of Fast Downward as covered by this
license:

* ./src/search/ext

For the rest, the following license applies:

```
Fast Downward is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Fast Downward is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.
```