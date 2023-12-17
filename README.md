# Changing Process Scheduler from RR to the Completely Fair Scheduler(CFS)

Kindly go through the presentation slides along with the README file to better understand the implementation through visual representations and explanations.

**Presentation Slides Link:** [Slides](https://docs.google.com/presentation/d/1HegY85bpJztjZwYOP5Y0NfYT1W5NtM1ZAfBmo98Jvz4/edit?usp=sharing)

## 1. Update in Kernel/Proc.h:
The following fields were added to the process struct to account for the CFS algo and RBT:
1. `int weight`:
2. `int v_runtime`       
3. `int niceness`  
4. `int current_run`       
5. `int max_exectime;`
6. `enum procColor node_color;`
7. `struct proc *left_node;`
8. `struct proc *right_node;`
9. `struct proc *parent_node;`

## 2. Functions Added to Kernel/Proc.c
<img width="815" alt="Screenshot 2023-12-17 at 12 29 55 AM" src="https://github.com/vgandhi13/XV6-Riscv-CFS/assets/82404434/a9220d9b-59b0-48cf-b59e-abb2588881aa">

The following functions were added to the proc.c file:

1. `void insertProcess(struct redblackTree* tree, struct proc* p)`: Inserts a new process `p` into a red-black tree (`tree`) This function is invoked in critical sections of the operating system, such as during the initialization of the first user process (`userinit`), the creation of a new process (`fork`), yielding the CPU to another process (`yield`), waking up processes from sleep (`wakeup`), and terminating a process (`kill`):
2. `void treenode_insertion(struct proc* curProc, struct proc* newProc)`: This is a Depth First Search algorithm function that checks where to insert the new process in the red black tree based on the virtual runtime.
3. `void updateInsertedProcessandTreeProperties(struct redblackTree* tree, struct proc* p)`: This function updates the minimum virtual runtime node that `tree->min_vRuntime` points to. It also calculates process weight and total weight of the tree based on the `niceness` value.
4. `void recolorAndRotate(struct redblackTree* tree, struct proc* p)`: Perform recoloring and rotation on the Red Black Tree to ensure all violations of RBT properties are solved.
5. `struct proc* getMinimumVRuntimeproc(struct proc* traversingProcess)`: Recursive Depth First Search function that finds the left-most node in the Red Black Tree.
6. `void updateTree(struct redblackTree* tree, struct proc* parent,struct proc* uncle,struct proc* grandpa)`: Flips the node colors of the parent, uncle and grandfather nodes of a particular node in the Red Black Tree.
7. `void LandLRSituations(struct redblackTree* tree, struct proc* p,struct proc* parent,struct proc* grandpa)`: Handles situations where new process is added to the left subtree.
8. `void RandRLSituations(struct redblackTree* tree, struct proc* p,struct proc* parent,struct proc* grandpa)`: Handles situations where new process is added to the right subtree.
9. `void L_Rotation(struct redblackTree* rbt, struct proc* curProc)`: Performs rotation such that the curProc is moved leftward in the Red Black Tree
10. `void R_Rotation(struct redblackTree* rbt, struct proc* curProc)`: Performs rotation such that the curProc is moved rightward in the Red Black Tree
11. `void handleDeletionOfLeftmostNodeAndUpdateTree(struct redblackTree* tree, struct proc* p)`: Handles the situation when the leftmost node in the Red Black Tree needs to be removed to be scheduled

Here is the code for the Rust library with the code for scheduler in Rust: https://github.com/vgandhi13/XV6-Rust-Library
## Running the Code

To run the code, follow these steps:

1. Download the Riscv toolchain: https://pdos.csail.mit.edu/6.S081/2021/tools.html
2. Run the command `make clean`.
3. Build the code by running `make`.
4. Execute the command `make qemu`.

If you have any questions regarding this project, feel free to email me at vgandhi@umass.edu.
