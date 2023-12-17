#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"


struct redblackTree {
 int num_of_nodes;
 int total_weight;
 struct proc *root;
 struct proc *min_vRuntime;
 struct spinlock lock;
 int period;
}rbTree;


static struct redblackTree *runnableprocRBTree = &rbTree;


//Set target scheduler latency and minimum granularity constants
//Latency must be multiples of min_granularity
static int latency = 32;
static int min_granularity = 2;
struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

void
L_Rotation(struct redblackTree* rbt, struct proc* curProc){
 struct proc* rightBranch = curProc->right_node;
 curProc->right_node = rightBranch->left_node;
 
 if(curProc->right_node != 0) {
  curProc->right_node->parent_node = curProc;
 }

 rightBranch->left_node = curProc;
 rightBranch->parent_node = curProc->parent_node;
 if (curProc->parent_node == 0) {
  rbt->root = rightBranch;
 } 
 else if (curProc->parent_node->left_node == curProc) {
  curProc->parent_node->left_node = rightBranch;
 }
 else {
  curProc->parent_node->right_node = rightBranch;
 }
 curProc->parent_node = rightBranch;
}


void
R_Rotation(struct redblackTree* rbt, struct proc* curProc){
 struct proc* leftBranch = curProc->left_node;
 curProc->left_node = leftBranch->right_node;
 
 if(curProc->left_node != 0) {
  curProc->left_node->parent_node = curProc;
 }

 leftBranch->right_node = curProc;
 leftBranch->parent_node = curProc->parent_node;
 if (curProc->parent_node == 0) {
  rbt->root = leftBranch;
 } 
 else if (curProc->parent_node->left_node == curProc) {
  curProc->parent_node->left_node = leftBranch;
 }
 else {
  curProc->parent_node->right_node = leftBranch;
 }
 curProc->parent_node = leftBranch;
}


/*
 Returns the leftmost node in the redblack tree
*/
struct proc*
getMinimumVRuntimeproc(struct proc* traversingProcess){
  if(traversingProcess != 0){
 if(traversingProcess->left_node != 0){
     return getMinimumVRuntimeproc(traversingProcess->left_node);
 } else {
     return traversingProcess;
 }
 }
 return 0;
}

/* NEW FUNCTIONS START*/
void recolorAndRotate(struct redblackTree* tree, struct proc* p);


void updateTree(struct redblackTree* tree, struct proc* parent,struct proc* uncle,struct proc* grandpa) {
 uncle->node_color = (uncle->node_color == RED)? BLACK:RED;
 parent->node_color = (parent->node_color == RED)? BLACK:RED;
 grandpa->node_color = (grandpa->node_color == RED)? BLACK:RED;
 recolorAndRotate(tree, grandpa); // move in an upwards manner from the node we inserted to the grandparent node     
}


void LandLRSituations(struct redblackTree* tree, struct proc* p,struct proc* parent,struct proc* grandpa) {
 if (p->left_node == 0) {  // for left right situation, we need to do a left rotation first, and then a right rotation 
   L_Rotation(tree, parent);
 }
 parent->node_color = (parent->node_color == RED)? BLACK:RED;
 grandpa->node_color = (grandpa->node_color == RED)? BLACK:RED;
 R_Rotation(tree, grandpa);  //if left heavy we just rotate right   
 if (p->left_node != 0) {  //in situation of just left, we hop on to the parent   
   recolorAndRotate(tree, p);
 }
 else { // in left right situation grandparent
   recolorAndRotate(tree, grandpa);
 }
}


void RandRLSituations(struct redblackTree* tree, struct proc* p,struct proc* parent,struct proc* grandpa) {
 if (p->left_node != 0) {
     R_Rotation(tree, parent);
   }
   parent->node_color = (parent->node_color == RED)? BLACK:RED;
   grandpa->node_color = (grandpa->node_color == RED)? BLACK:RED;
   L_Rotation(tree, grandpa);
   if (p->left_node != 0) {
     recolorAndRotate(tree, grandpa);
   }
   else {
     recolorAndRotate(tree, p);
   }
}


struct proc*
getuncle(struct proc* process){
 struct proc* grandpa = process->parent_node->parent_node;
 if(grandpa != 0){
 if(process->parent_node == grandpa->left_node){
   return grandpa->right_node;
 } else {
   return grandpa->left_node;
 }
 }
  return 0;
}



void recolorAndRotate(struct redblackTree* tree, struct proc* p) {
 struct proc* parent = p->parent_node;
 if(parent != 0 && parent->node_color == RED) {
   struct proc* grandpa = p->parent_node->parent_node;
   struct proc* uncle = getuncle(p); //if uncle is red we can stick with recoloring the nodes only. However, 
   //if uncle is a black node then we will have to rotate the nodes  
   if (uncle != 0 && uncle->node_color == RED) {
     updateTree(tree,parent, uncle, grandpa); //recoloring and rotating
   }
   else if (parent->left_node != 0) {
     LandLRSituations(tree, p, parent, grandpa); //left heavy or left-right situations
   }
   else if(parent->left_node == 0) {
     RandRLSituations(tree, p, parent, grandpa); //right heavy or right left situations
   }
 }
 tree->root->node_color = BLACK;
}


void
updateInsertedProcessandTreeProperties(struct redblackTree* tree, struct proc* p){

 double denominator = 1.25;
 if(p->niceness > 30){
 p->niceness = 30;
 }
 int iterator = 0;
 while (iterator < p->niceness && p->niceness > 0){
   denominator = denominator * 1.25;
 }
 p->weight = ((int) (1024/denominator));
 tree->total_weight += p->weight;  
 tree->num_of_nodes += 1;
 if(tree->min_vRuntime == 0 || tree->min_vRuntime->left_node != 0) {
   tree->min_vRuntime = getMinimumVRuntimeproc(tree->root); //updating the minimum vruntime node of the tree
 }
}



/*
This is a Depth First Search algorithm function that checks where to insert the new process in 
the red black tree based on the virtual runtime*/
struct proc*
treenode_insertion(struct proc* curProc, struct proc* newProc){
  newProc->node_color = RED; //each node when being inserted is red according to redblack tree property
 if(curProc == 0){//if no node exists in tree, or one of the null nodes in the tree reached
 return newProc;
 }  

 //condition compares the run time of current node and the node being inserted
 if(curProc->v_runtime <= newProc->v_runtime){
 newProc->parent_node = curProc;
 curProc->right_node = treenode_insertion(curProc->right_node, newProc);
 } else {
 newProc->parent_node = curProc;   
 curProc->left_node = treenode_insertion(curProc->left_node, newProc);
 }
return curProc;
}

void
insertProcess(struct redblackTree* tree, struct proc* p){
 acquire(&tree->lock);
 tree->root = treenode_insertion(tree->root, p); //inserts node in rb tree while maintaining rb tree rules
 if(tree->num_of_nodes == 0){
   tree->root->parent_node = 0;
 }
 updateInsertedProcessandTreeProperties(tree, p);
 recolorAndRotate(tree, p); //Checking for possible red black tree violations after insertion of the new process
 
 release(&tree->lock);
}

/*DELETION NEW FUNCTIONS START*/


//Possible scenarios
//situation 1. p is the root, if it has a right node, make that the root, else tree is completely empty. two cases right 0 or not 0
//situation 2. p is black, parent is red, p right child is red
//situation 3. p is black, parent is black, p right child is black
//situation 4. p is red, parent is black, child is black
//situation 5: right child not there, do nothing
void handleDeletionOfLeftmostNodeAndUpdateTree(struct redblackTree* tree, struct proc* p) {
      // situation 1

      if (p->parent_node == 0) {
        if (p->right_node == 0) {
          tree->root = 0;
        }
        else {
          p->right_node->parent_node = p->parent_node;
          tree->root = p->right_node;
          p->right_node->node_color = BLACK;
        }
      }


      // situation 5
      else if (p->right_node == 0) { //if p not root, and right child doesn't exits, nothing to replace with
        p->parent_node->left_node = 0;
      }
      //SITUATION 2
      else if (p->parent_node->node_color == RED && p->node_color == BLACK && p->right_node->node_color == RED) {
        p->parent_node->left_node = p->right_node;
        p->right_node->node_color = BLACK; //because RED Node can't have a red child
        p->right_node->parent_node = p->parent_node;
      }
      //SITUATION 3 & 4
      else {
        p->parent_node->left_node = p->right_node;
        p->right_node->parent_node = p->parent_node;
      }
      p->left_node = 0;
      p->parent_node = 0;
      p->right_node = 0;
      tree->num_of_nodes -= 1;
      tree->min_vRuntime = getMinimumVRuntimeproc(tree->root);//Update the field with process with the smallest virtual runtime
      //The formula can be found in CFS tuning article by Jacek Kobus and Refal Szklarski in the scheduling section:
      p->max_exectime = (tree->period * p->weight / tree->total_weight);
        //Recalculate total weight of red-black tree
      tree->total_weight -= p->weight;
       //The formula can be found in CFS tuning article by Jacek Kobus and Refal Szklarski. In the CFS schduler tuning section
      if(tree->num_of_nodes > (latency / min_granularity)){
        tree->period = tree->num_of_nodes * min_granularity;
      }
  }


/*DELETION NEW FUNCTIONS END*/

struct proc*
getProcessToRunFromTree(struct redblackTree* tree){
 struct proc* foundProcess;  //Process pointer utilized to hold the address of the process with smallest VRuntime
 acquire(&tree->lock);
 //retrive the process with the smallest virtual runtime by removing it from the red black tree and returning it
 foundProcess = tree->min_vRuntime; //0 if no node in tree
 if(!(foundProcess == 0)){  //checks if tree is not empty
 
 //Determine if the process that is being chosen is runnable at the time of the selection, if it is not, then don't return it.
 if(foundProcess->state != RUNNABLE){
   release(&tree->lock);
   return 0;
 }
 handleDeletionOfLeftmostNodeAndUpdateTree(tree, tree->min_vRuntime);
 }

 release(&tree->lock);
 return foundProcess;
}


int
preemptionRequired(struct proc* curProc, struct redblackTree* tree){
 if(((curProc->current_run >= curProc->max_exectime) && (curProc->current_run >= min_granularity))){
   return 1;
 }
 else if ((tree->min_vRuntime != 0 && tree->min_vRuntime->state == RUNNABLE && curProc->v_runtime > tree->min_vRuntime->v_runtime)) {
  if (((curProc->current_run != 0) && (curProc->current_run >= min_granularity)) || (curProc->current_run == 0)) {
    return 1;
  }
 }
 return 0;
}


// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
 struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++) {
   char *pa = kalloc();
   if(pa == 0)
     panic("kalloc");
   uint64 va = KSTACK((int) (p - proc));
   kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
 }
}

// initialize the proc table.
void
procinit(void)
{
 struct proc *p;
  initlock(&pid_lock, "nextpid");
 initlock(&wait_lock, "wait_lock");
 for(p = proc; p < &proc[NPROC]; p++) {
     initlock(&p->lock, "proc");
     p->state = UNUSED;
     p->kstack = KSTACK((int) (p - proc));
 }
 initlock(&runnableprocRBTree->lock, "runnableprocRBTree");
 runnableprocRBTree->num_of_nodes = 0;
 runnableprocRBTree->root = 0;
 runnableprocRBTree->period = latency;
 runnableprocRBTree->total_weight = 0;
 runnableprocRBTree->min_vRuntime = 0;
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
 int id = r_tp();
 return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
 int id = cpuid();
 struct cpu *c = &cpus[id];
 return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
 push_off();
 struct cpu *c = mycpu();
 struct proc *p = c->proc;
 pop_off();
 return p;
}

int
allocpid()
{
 int pid;
  acquire(&pid_lock);
 pid = nextpid;
 nextpid = nextpid + 1;
 release(&pid_lock);

 return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
 struct proc *p;

 for(p = proc; p < &proc[NPROC]; p++) {
   acquire(&p->lock);
   if(p->state == UNUSED) {
     goto found;
   } else {
     release(&p->lock);
   }
 }
 return 0;

found:
 p->pid = allocpid();
 p->state = USED;


 p->v_runtime = 0;
 p->current_run = 0;
 p->max_exectime = 0;
 p->niceness = 0;


 p->left_node = 0;
 p->right_node = 0;
 p->parent_node = 0;

 // Allocate a trapframe page.
 if((p->trapframe = (struct trapframe *)kalloc()) == 0){
   freeproc(p);
   release(&p->lock);
   return 0;
 }

 // An empty user page table.
 p->pagetable = proc_pagetable(p);
 if(p->pagetable == 0){
   freeproc(p);
   release(&p->lock);
   return 0;
 }

 // Set up new context to start executing at forkret,
 // which returns to user space.
 memset(&p->context, 0, sizeof(p->context));
 p->context.ra = (uint64)forkret;
 p->context.sp = p->kstack + PGSIZE;

 return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
 if(p->trapframe)
   kfree((void*)p->trapframe);
 p->trapframe = 0;
 if(p->pagetable)
   proc_freepagetable(p->pagetable, p->sz);
 p->pagetable = 0;
 p->sz = 0;
 p->pid = 0;
 p->parent = 0;
 p->name[0] = 0;
 p->chan = 0;
 p->killed = 0;
 p->xstate = 0;
 p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
 pagetable_t pagetable;

 // An empty page table.
 pagetable = uvmcreate();
 if(pagetable == 0)
   return 0;

 // map the trampoline code (for system call return)
 // at the highest user virtual address.
 // only the supervisor uses it, on the way
 // to/from user space, so not PTE_U.
 if(mappages(pagetable, TRAMPOLINE, PGSIZE,
             (uint64)trampoline, PTE_R | PTE_X) < 0){
   uvmfree(pagetable, 0);
   return 0;
 }

 // map the trapframe page just below the trampoline page, for
 // trampoline.S.
 if(mappages(pagetable, TRAPFRAME, PGSIZE,
             (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
   uvmunmap(pagetable, TRAMPOLINE, 1, 0);
   uvmfree(pagetable, 0);
   return 0;
 }

 return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
 uvmunmap(pagetable, TRAMPOLINE, 1, 0);
 uvmunmap(pagetable, TRAPFRAME, 1, 0);
 uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
 0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
 0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
 0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
 0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
 0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
 0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
 struct proc *p;

 p = allocproc();
 initproc = p;
  // allocate one user page and copy initcode's instructions
 // and data into it.
 uvmfirst(p->pagetable, initcode, sizeof(initcode));
 p->sz = PGSIZE;

 // prepare for the very first "return" from kernel to user.
 p->trapframe->epc = 0;      // user program counter
 p->trapframe->sp = PGSIZE;  // user stack pointer

 safestrcpy(p->name, "initcode", sizeof(p->name));
 p->cwd = namei("/");

 p->state = RUNNABLE;

 // Red Black Tree is empty before this function call
 //Insert allocated process into the red black tree, which will become runnable, i.e its state will be set to runnable, after this function returns to the caller.
 insertProcess(runnableprocRBTree, p);
 release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
 uint64 sz;
 struct proc *p = myproc();

 sz = p->sz;
 if(n > 0){
   if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
     return -1;
   }
 } else if(n < 0){
   sz = uvmdealloc(p->pagetable, sz, sz + n);
 }
 p->sz = sz;
 return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
 int i, pid;
 struct proc *np;
 struct proc *p = myproc();

 // Allocate process.
 if((np = allocproc()) == 0){
   return -1;
 }

 // Copy user memory from parent to child.
 if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
   freeproc(np);
   release(&np->lock);
   return -1;
 }
 np->sz = p->sz;

 // copy saved user registers.
 *(np->trapframe) = *(p->trapframe);

 // Cause fork to return 0 in the child.
 np->trapframe->a0 = 0;

 // increment reference counts on open file descriptors.
 for(i = 0; i < NOFILE; i++)
   if(p->ofile[i])
     np->ofile[i] = filedup(p->ofile[i]);
 np->cwd = idup(p->cwd);

 safestrcpy(np->name, p->name, sizeof(p->name));

 pid = np->pid;

 release(&np->lock);

 acquire(&wait_lock);
 np->parent = p;
 release(&wait_lock);

 acquire(&np->lock);
 np->state = RUNNABLE;
 release(&np->lock);

 //Insert allocated process into the red black tree, which will become runnable, i.e its state will be set to runnable, after this function returns to the caller.
 insertProcess(runnableprocRBTree, np);
 return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
 struct proc *pp;

 for(pp = proc; pp < &proc[NPROC]; pp++){
   if(pp->parent == p){
     pp->parent = initproc;
     wakeup(initproc);
   }
 }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
 struct proc *p = myproc();

 if(p == initproc)
   panic("init exiting");

 // Close all open files.
 for(int fd = 0; fd < NOFILE; fd++){
   if(p->ofile[fd]){
     struct file *f = p->ofile[fd];
     fileclose(f);
     p->ofile[fd] = 0;
   }
 }

 begin_op();
 iput(p->cwd);
 end_op();
 p->cwd = 0;

 acquire(&wait_lock);

 // Give any children to init.
 reparent(p);

 // Parent might be sleeping in wait().
 wakeup(p->parent);
  acquire(&p->lock);

 p->xstate = status;
 p->state = ZOMBIE;

 release(&wait_lock);

 // Jump into the scheduler, never to return.
 sched();
 panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
 struct proc *pp;
 int havekids, pid;
 struct proc *p = myproc();

 acquire(&wait_lock);

 for(;;){
   // Scan through table looking for exited children.
   havekids = 0;
   for(pp = proc; pp < &proc[NPROC]; pp++){
     if(pp->parent == p){
       // make sure the child isn't still in exit() or swtch().
       acquire(&pp->lock);

       havekids = 1;
       if(pp->state == ZOMBIE){
         // Found one.
         pid = pp->pid;
         if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                 sizeof(pp->xstate)) < 0) {
           release(&pp->lock);
           release(&wait_lock);
           return -1;
         }
         freeproc(pp);
         release(&pp->lock);
         release(&wait_lock);
         return pid;
       }
       release(&pp->lock);
     }
   }

   // No point waiting if we don't have any children.
   if(!havekids || killed(p)){
     release(&wait_lock);
     return -1;
   }
  
   // Wait for a child to exit.
   sleep(p, &wait_lock);  //DOC: wait-sleep
 }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
 struct proc *p;
 struct cpu *c = mycpu();
  c->proc = 0;
 for(;;){
   // Avoid deadlock by ensuring that devices can interrupt.
   intr_on();
   p = getProcessToRunFromTree(runnableprocRBTree);

   while(p != 0){
     acquire(&p->lock);
     if(p->state == RUNNABLE) {
       // Switch to chosen process.  It is the process's job
       // to release its lock and then reacquire it
       // before jumping back to us.
       p->state = RUNNING;
       c->proc = p;
       swtch(&c->context, &p->context);


       // Process is done running for now.
       // It should have changed its p->state before coming back.
       c->proc = 0;
     }
     release(&p->lock);
     p = getProcessToRunFromTree(runnableprocRBTree);
   }
 }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
 int intena;
 struct proc *p = myproc();

 if(!holding(&p->lock))
   panic("sched p->lock");
 if(mycpu()->noff != 1)
   panic("sched locks");
 if(p->state == RUNNING)
   panic("sched running");
 if(intr_get())
   panic("sched interruptible");

 intena = mycpu()->intena;
 swtch(&p->context, &mycpu()->context);
 mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
 struct proc *p = myproc();
 acquire(&p->lock);
 // p->state = RUNNABLE;
 // sched();
 if(preemptionRequired(p, runnableprocRBTree)){  //if condition satisfied, we need to make the current running process to stop and put it back on redblack tree
   p->state = RUNNABLE;
   p->v_runtime = p->v_runtime + p->current_run;
   p->current_run = 0;
   insertProcess(runnableprocRBTree, myproc());
   sched();
 }
 release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
 static int first = 1;

 // Still holding p->lock from scheduler.
 release(&myproc()->lock);

 if (first) {
   // File system initialization must be run in the context of a
   // regular process (e.g., because it calls sleep), and thus cannot
   // be run from main().
   first = 0;
   fsinit(ROOTDEV);
 }

 usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
 struct proc *p = myproc();

  // Must acquire p->lock in order to
 // change p->state and then call sched.
 // Once we hold p->lock, we can be
 // guaranteed that we won't miss any wakeup
 // (wakeup locks p->lock),
 // so it's okay to release lk.

 acquire(&p->lock);  //DOC: sleeplock1
 release(lk);

 // Go to sleep.
 p->chan = chan;
 p->state = SLEEPING;

 sched();

 // Tidy up.
 p->chan = 0;

 // Reacquire original lock.
 release(&p->lock);
 acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
 struct proc *p;

 for(p = proc; p < &proc[NPROC]; p++) {
   if(p != myproc()){
     acquire(&p->lock);
     if(p->state == SLEEPING && p->chan == chan) {
       p->state = RUNNABLE;
       //Update runtime stats of process being woken up
       p->v_runtime = p->v_runtime + p->current_run;
       p->current_run = 0;


       //Insert process after it has finished Sleeping
       insertProcess(runnableprocRBTree, p);
     }
     release(&p->lock);
   }
 }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
 struct proc *p;

 for(p = proc; p < &proc[NPROC]; p++){
   acquire(&p->lock);
   if(p->pid == pid){
     p->killed = 1;
     if(p->state == SLEEPING){
       // Wake process from sleep().
       p->state = RUNNABLE;
        //Update runtime stats of process being killed
       p->v_runtime = p->v_runtime + p->current_run;
       p->current_run = 0;


       //insert process into runnableTask tree
       insertProcess(runnableprocRBTree, p);
     }
     release(&p->lock);
     return 0;
   }
   release(&p->lock);
 }
 return -1;
}

void
setkilled(struct proc *p)
{
 acquire(&p->lock);
 p->killed = 1;
 release(&p->lock);
}

int
killed(struct proc *p)
{
 int k;

 acquire(&p->lock);
 k = p->killed;
 release(&p->lock);
 return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
 struct proc *p = myproc();
 if(user_dst){
   return copyout(p->pagetable, dst, src, len);
 } else {
   memmove((char *)dst, src, len);
   return 0;
 }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
 struct proc *p = myproc();
 if(user_src){
   return copyin(p->pagetable, dst, src, len);
 } else {
   memmove(dst, (char*)src, len);
   return 0;
 }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
 static char *states[] = {
 [UNUSED]    "unused",
 [USED]      "used",
 [SLEEPING]  "sleep ",
 [RUNNABLE]  "runble",
 [RUNNING]   "run   ",
 [ZOMBIE]    "zombie"
 };
 struct proc *p;
 char *state;

 printf("\n");
 for(p = proc; p < &proc[NPROC]; p++){
   if(p->state == UNUSED)
     continue;
   if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
     state = states[p->state];
   else
     state = "???";
   printf("%d %s %s", p->pid, state, p->name);
   printf("\n");
 }
}
