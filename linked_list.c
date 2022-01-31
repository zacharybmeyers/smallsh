/*
    GLOBAL LINKED LIST FOR BACKGROUND PROCESSES AND ALL METHODS
        --add_bg_process()
        --remove_bg_head()
        --remove_bg_tail()
        --remove_bg_process()
        --free_bg_list()
        --print_bg_list()
*/

struct bg_process {
    int bg_pid;
    struct bg_process *prev;
    struct bg_process *next;
};

struct bg_process *bg_head = NULL;
struct bg_process *bg_tail = NULL;

void add_bg_process(int pid) {
    // create new node
    struct bg_process *new_node = malloc(sizeof(struct bg_process));
    new_node->bg_pid = pid;
    new_node->prev = NULL;
    new_node->next = NULL;
    
    // if no head, set head and tail
    if (bg_head == NULL && bg_tail == NULL) {
        bg_head = new_node;
        bg_tail = new_node;
    } 
    // otherwise, add to end of list
    else {
        // end of list, add new node
        bg_tail->next = new_node;
        new_node->prev = bg_tail;
        bg_tail = new_node;
    }
}

void remove_bg_head() {
    // if head is only node
    if (bg_head->next == NULL && bg_head->prev == NULL) {
        free(bg_head);
        bg_head = NULL;
        bg_tail = NULL;
    } 
    // otherwise, move head to next node
    else {
        struct bg_process *temp;
        temp = bg_head;
        bg_head = temp->next;
        bg_head->prev = NULL;

        temp->next = NULL;
        free(temp);
    }
}

void remove_bg_tail() {
    // if tail is only node
    if (bg_tail->next == NULL && bg_tail->prev == NULL) {
        free(bg_tail);
        bg_tail = NULL;
        bg_head = NULL;
    }
    // otherwise, move tail to previous node
    else {
        struct bg_process *temp;
        temp = bg_tail;
        bg_tail = temp->prev;
        bg_tail->next = NULL;

        temp->prev = NULL;
        free(temp);
    }    
}

void remove_bg_process(int pid) {
    // no processes to remove
    if (bg_head == NULL && bg_tail == NULL) {
        return;
    }
    // if head node
    if (bg_head->bg_pid == pid) {
        remove_bg_head();
    }
    // if tail node
    else if (bg_tail->bg_pid == pid) {
        remove_bg_tail();
    }
    // if middle node
    else {
        // get current node
        struct bg_process *curr;
        curr = bg_head;

        // loop until curr node is the removal node
        while (curr->bg_pid != pid) {
            curr = curr->next;
        }

        // create pointers to left and right of curr
        struct bg_process *left;
        left = curr->prev;
        struct bg_process *right;
        right = curr->next;

        // point over removal node
        left->next = right;
        right->prev = left;

        // free removal node
        free(curr);
        curr->prev = NULL;
        curr->next = NULL;
    }
    
}

void free_bg_list() {
    // get head
    struct bg_process *curr;
    curr = bg_head;

    // if there's at least one node
    while (curr) {
        // store next
        struct bg_process *next;
        next = curr->next;

        // kill process
        kill(curr->bg_pid, SIGTERM);

        // free curr node
        free(curr);
        curr->prev = NULL;
        curr->next = NULL;
        // update curr
        curr = next;
    }
}

void print_bg_list() {
    struct bg_process *curr;
    curr = bg_head;

    printf("NULL ");
    while (curr) {
        printf("<- %d -> ", curr->bg_pid);
        curr = curr->next;
    }
    printf("NULL\n");
}