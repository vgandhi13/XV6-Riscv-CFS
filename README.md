# Changing Process Scheduler from RR to the Completely Fair Scheduler(CFS)

Kindly go through the presentation slides along with the README file to better understand the implementation through visual representations and explanations.

**Presentation Slides Link:** [Slides](https://docs.google.com/presentation/d/1HegY85bpJztjZwYOP5Y0NfYT1W5NtM1ZAfBmo98Jvz4/edit?usp=sharing)


## 1. Functions Added to Proc.c

The following functions were added to the proc.c file:

1. `void insertProcess(struct redblackTree* tree, struct proc* p)`: Inserts a new process `p` into a red-black tree (`tree`) This function is invoked in critical sections of the operating system, such as during the initialization of the first user process (`userinit`), the creation of a new process (`fork`), yielding the CPU to another process (`yield`), waking up processes from sleep (`wakeup`), and terminating a process (`kill`):
2. `void treenode_insertion(struct proc* curProc, struct proc* newProc)`: This is a Depth First Search algorithm function that checks where to insert the new process in the red black tree based on the virtual runtime.
3. `void updateInsertedProcessandTreeProperties(struct redblackTree* tree, struct proc* p)`: This function updates the minimum virtual runtime node that `tree->min_vRuntime` points to. It also calculates process weight and total weight of the tree based on the `niceness` value.
4. `void recolorAndRotate(struct redblackTree* tree, struct proc* p)`: Perform recoloring and rotation on the Red Black Tree to ensure all violations of RBT properties are solved.
5. `struct proc* getMinimumVRuntimeproc(struct proc* traversingProcess)`: Recursive Depth First Search function that finds the left-most node in the Red Black Tree.
## Running the Code

To run the code, follow these steps:

1. Clone the repository or download the zip file.
2. Build the code by running `make`.
3. Execute the command `./allocator_app`.
4. The results for both `my_malloc.cpp` and `buddy_malloc.cpp` will be displayed.

If you have any questions regarding this project, feel free to email me at vgandhi@umass.edu.
