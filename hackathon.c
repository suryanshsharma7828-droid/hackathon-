#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define BOOKS_FILE "library_books.dat"
#define MEMBERS_FILE "library_members.dat"
#define LOANS_FILE "library_loans.dat"
#define MAX_WAITLIST 10
#define LOAN_DAYS_STUDENT 14
#define LOAN_DAYS_FACULTY 30
#define MAX_BOOKS_STUDENT 3
#define MAX_BOOKS_FACULTY 6
#define FINE_PER_DAY 5.00
#define LIBRARIAN_PASSCODE 8888

typedef struct {
    int bookId;
    char title[80];
    char author[60];
    char category[32];     // CS/IT, Fiction, Science, Electronics, Management
    char isbn[24];
    char shelfCode[16];    // e.g., RACK-A3
    int totalCopies;
    int availableCopies;
    int timesBorrowed;
    double ratingSum;
    int ratingCount;
    int waitlistCount;
    int waitlistMemberIds[MAX_WAITLIST];
    int isActive;
} Book;

typedef struct {
    int memberId;
    char name[60];
    char role[16];         // Student or Faculty
    char department[32];
    char contact[16];
    int pin;
    int activeLoansCount;
    double pendingFines;
    double totalFinesPaid;
    int isBlocked;
    int isActive;
} Member;

typedef struct {
    int loanId;
    int bookId;
    int memberId;
    time_t issueEpoch;
    time_t dueEpoch;
    time_t returnEpoch;
    char issueDateStr[24];
    char dueDateStr[24];
    char returnDateStr[24];
    int renewalsUsed;
    double fineCharged;
    int isReturned;
} LoanRecord;

/* -------------------- General Helpers -------------------- */

void clearInputBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

void formatEpochDate(time_t t, char *buf, size_t size) {
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M", tm_info);
}

int containsCaseInsensitive(const char *haystack, const char *needle) {
    if (!needle || needle[0] == '\0') return 1;
    char h[128], n[128];
    size_t i;
    for (i = 0; i < sizeof(h) - 1 && haystack[i]; i++) {
        h[i] = (char)tolower((unsigned char)haystack[i]);
    }
    h[i] = '\0';
    for (i = 0; i < sizeof(n) - 1 && needle[i]; i++) {
        n[i] = (char)tolower((unsigned char)needle[i]);
    }
    n[i] = '\0';
    return strstr(h, n) != NULL;
}

int getMaxAllowedBooks(const Member *m) {
    return (strcmp(m->role, "Faculty") == 0) ? MAX_BOOKS_FACULTY : MAX_BOOKS_STUDENT;
}

int getAllowedLoanDays(const Member *m) {
    return (strcmp(m->role, "Faculty") == 0) ? LOAN_DAYS_FACULTY : LOAN_DAYS_STUDENT;
}

double calculateOverdueFine(time_t dueEpoch, time_t refEpoch) {
    if (refEpoch <= dueEpoch) return 0.0;
    double diffSeconds = difftime(refEpoch, dueEpoch);
    int overdueDays = (int)(diffSeconds / 86400.0);
    if (overdueDays < 1) overdueDays = 1;
    return overdueDays * FINE_PER_DAY;
}

/* -------------------- File Storage Helpers -------------------- */

int nextBookId(void) {
    FILE *fp = fopen(BOOKS_FILE, "rb");
    int maxId = 1000;
    if (fp) {
        Book b;
        while (fread(&b, sizeof(Book), 1, fp) == 1) {
            if (b.bookId > maxId) maxId = b.bookId;
        }
        fclose(fp);
    }
    return maxId + 1;
}

int nextMemberId(void) {
    FILE *fp = fopen(MEMBERS_FILE, "rb");
    int maxId = 5000;
    if (fp) {
        Member m;
        while (fread(&m, sizeof(Member), 1, fp) == 1) {
            if (m.memberId > maxId) maxId = m.memberId;
        }
        fclose(fp);
    }
    return maxId + 1;
}

int nextLoanId(void) {
    FILE *fp = fopen(LOANS_FILE, "rb");
    int maxId = 90000;
    if (fp) {
        LoanRecord lr;
        while (fread(&lr, sizeof(LoanRecord), 1, fp) == 1) {
            if (lr.loanId > maxId) maxId = lr.loanId;
        }
        fclose(fp);
    }
    return maxId + 1;
}

int findBookById(int bookId, Book *outBook, long *outOffset) {
    FILE *fp = fopen(BOOKS_FILE, "rb");
    if (!fp) return 0;
    Book temp;
    long offset = 0;
    while (fread(&temp, sizeof(Book), 1, fp) == 1) {
        if (temp.bookId == bookId && temp.isActive) {
            *outBook = temp;
            if (outOffset) *outOffset = offset;
            fclose(fp);
            return 1;
        }
        offset = ftell(fp);
    }
    fclose(fp);
    return 0;
}

int updateBookAtOffset(const Book *b, long offset) {
    FILE *fp = fopen(BOOKS_FILE, "r+b");
    if (!fp) return 0;
    fseek(fp, offset, SEEK_SET);
    int ok = (fwrite(b, sizeof(Book), 1, fp) == 1);
    fclose(fp);
    return ok;
}

int appendBook(const Book *b) {
    FILE *fp = fopen(BOOKS_FILE, "ab");
    if (!fp) return 0;
    int ok = (fwrite(b, sizeof(Book), 1, fp) == 1);
    fclose(fp);
    return ok;
}

int findMemberById(int memberId, Member *outMember, long *outOffset) {
    FILE *fp = fopen(MEMBERS_FILE, "rb");
    if (!fp) return 0;
    Member temp;
    long offset = 0;
    while (fread(&temp, sizeof(Member), 1, fp) == 1) {
        if (temp.memberId == memberId && temp.isActive) {
            *outMember = temp;
            if (outOffset) *outOffset = offset;
            fclose(fp);
            return 1;
        }
        offset = ftell(fp);
    }
    fclose(fp);
    return 0;
}

int updateMemberAtOffset(const Member *m, long offset) {
    FILE *fp = fopen(MEMBERS_FILE, "r+b");
    if (!fp) return 0;
    fseek(fp, offset, SEEK_SET);
    int ok = (fwrite(m, sizeof(Member), 1, fp) == 1);
    fclose(fp);
    return ok;
}

int appendMember(const Member *m) {
    FILE *fp = fopen(MEMBERS_FILE, "ab");
    if (!fp) return 0;
    int ok = (fwrite(m, sizeof(Member), 1, fp) == 1);
    fclose(fp);
    return ok;
}

int appendLoanRecord(const LoanRecord *lr) {
    FILE *fp = fopen(LOANS_FILE, "ab");
    if (!fp) return 0;
    int ok = (fwrite(lr, sizeof(LoanRecord), 1, fp) == 1);
    fclose(fp);
    return ok;
}

int updateLoanAtOffset(const LoanRecord *lr, long offset) {
    FILE *fp = fopen(LOANS_FILE, "r+b");
    if (!fp) return 0;
    fseek(fp, offset, SEEK_SET);
    int ok = (fwrite(lr, sizeof(LoanRecord), 1, fp) == 1);
    fclose(fp);
    return ok;
}

/* -------------------- 50-Book Catalog Engine -------------------- */

#define INITIAL_BOOK_COUNT 50
static Book runtimeBooks[200];
static int runtimeBookCount = 0;

typedef struct {
    const char *title;
    const char *author;
    const char *category;
    const char *isbn;
    const char *shelfCode;
    int copies;
    double ratingSum;
    int ratingCount;
} SeedBookSpec;

static const SeedBookSpec INITIAL_50_BOOKS[INITIAL_BOOK_COUNT] = {
    {"The C Programming Language", "Brian W. Kernighan & Dennis Ritchie", "CS/IT", "978-0131103627", "RACK-CS01", 3, 19.5, 4},
    {"Introduction to Algorithms (CLRS)", "Thomas H. Cormen", "CS/IT", "978-0262046305", "RACK-CS02", 3, 14.4, 3},
    {"Clean Code", "Robert C. Martin", "Software Eng", "978-0132350884", "RACK-SE01", 3, 9.6, 2},
    {"Atomic Habits", "James Clear", "Self-Growth", "978-0735211292", "RACK-GN01", 2, 4.9, 1},
    {"Data Structures and Algorithms in C", "Adam Drozdek", "CS/IT", "978-1133608424", "RACK-CS03", 3, 13.8, 3},
    {"Operating System Concepts", "Abraham Silberschatz", "CS/IT", "978-1119800361", "RACK-CS04", 3, 18.4, 4},
    {"Computer Networking: Top-Down Approach", "James F. Kurose", "CS/IT", "978-0136681557", "RACK-CS05", 2, 9.4, 2},
    {"Database System Concepts", "Henry F. Korth", "CS/IT", "978-0078022159", "RACK-CS06", 3, 14.1, 3},
    {"Artificial Intelligence: Modern Approach", "Stuart Russell & Peter Norvig", "AI/ML", "978-0134610993", "RACK-AI01", 3, 19.2, 4},
    {"Deep Learning", "Ian Goodfellow & Yoshua Bengio", "AI/ML", "978-0262035613", "RACK-AI02", 2, 9.5, 2},
    {"Hands-On Machine Learning", "Aurelien Geron", "AI/ML", "978-1098125974", "RACK-AI03", 3, 14.7, 3},
    {"Pattern Recognition and Machine Learning", "Christopher M. Bishop", "AI/ML", "978-0387310732", "RACK-AI04", 2, 9.2, 2},
    {"Design Patterns: GoF", "Erich Gamma et al.", "Software Eng", "978-0201633610", "RACK-SE02", 2, 9.6, 2},
    {"The Pragmatic Programmer", "Andrew Hunt & David Thomas", "Software Eng", "978-0135957059", "RACK-SE03", 3, 14.8, 3},
    {"Refactoring", "Martin Fowler", "Software Eng", "978-0134757599", "RACK-SE04", 2, 9.3, 2},
    {"System Design Interview Vol. 1", "Alex Xu", "Software Eng", "978-1736049112", "RACK-SE05", 3, 14.5, 3},
    {"Designing Data-Intensive Applications", "Martin Kleppmann", "Software Eng", "978-1449373320", "RACK-SE06", 3, 19.8, 4},
    {"Compilers: Principles & Tools (Dragon)", "Alfred V. Aho", "CS/IT", "978-0321486813", "RACK-CS07", 2, 9.1, 2},
    {"Computer Organization and Design", "David A. Patterson", "CS/IT", "978-0128201091", "RACK-CS08", 3, 13.9, 3},
    {"Discrete Mathematics and Applications", "Kenneth H. Rosen", "Mathematics", "978-1259676512", "RACK-MT01", 3, 14.0, 3},
    {"Linear Algebra and Its Applications", "Gilbert Strang", "Mathematics", "978-0030105678", "RACK-MT02", 2, 9.7, 2},
    {"Higher Engineering Mathematics", "B.S. Grewal", "Mathematics", "978-8174091956", "RACK-MT03", 3, 18.8, 4},
    {"Probability and Statistics for Engineers", "Ronald E. Walpole", "Mathematics", "978-0134115856", "RACK-MT04", 2, 9.0, 2},
    {"Digital Design", "M. Morris Mano", "Electronics", "978-0134549897", "RACK-EC01", 3, 14.3, 3},
    {"Microelectronic Circuits", "Adel S. Sedra & Kenneth Smith", "Electronics", "978-0190853464", "RACK-EC02", 2, 9.4, 2},
    {"Signals and Systems", "Alan V. Oppenheim", "Electronics", "978-0138147570", "RACK-EC03", 2, 9.2, 2},
    {"Embedded Systems Architecture", "Tammy Noergaard", "Electronics", "978-0123821966", "RACK-EC04", 2, 8.9, 2},
    {"Engineering Mechanics: Statics & Dynamics", "R.C. Hibbeler", "Mechanical", "978-0133915426", "RACK-ME01", 3, 13.7, 3},
    {"Thermodynamics: An Engineering Approach", "Yunus A. Cengel", "Mechanical", "978-1259822674", "RACK-ME02", 3, 14.2, 3},
    {"Fluid Mechanics", "Frank M. White", "Mechanical", "978-1260258318", "RACK-ME03", 2, 9.1, 2},
    {"Concepts of Physics Vol. 1", "H.C. Verma", "Science", "978-8177091878", "RACK-SC01", 3, 19.6, 4},
    {"Concepts of Physics Vol. 2", "H.C. Verma", "Science", "978-8177092325", "RACK-SC02", 3, 19.5, 4},
    {"Feynman Lectures on Physics Vol. 1", "Richard P. Feynman", "Science", "978-0465024933", "RACK-SC03", 2, 9.9, 2},
    {"A Brief History of Time", "Stephen Hawking", "Science", "978-0553380163", "RACK-SC04", 3, 14.6, 3},
    {"Sapiens: A Brief History of Humankind", "Yuval Noah Harari", "History", "978-0062316097", "RACK-HS01", 3, 14.4, 3},
    {"Wings of Fire", "A.P.J. Abdul Kalam", "Biography", "978-8173711466", "RACK-BG01", 3, 19.7, 4},
    {"Ignited Minds", "A.P.J. Abdul Kalam", "Biography", "978-0143424123", "RACK-BG02", 2, 9.6, 2},
    {"Zero to One", "Peter Thiel", "Management", "978-0804139298", "RACK-MG01", 3, 14.1, 3},
    {"The Lean Startup", "Eric Ries", "Management", "978-0307887894", "RACK-MG02", 2, 9.2, 2},
    {"Principles of Management", "Harold Koontz", "Management", "978-9353166793", "RACK-MG03", 2, 8.8, 2},
    {"Rich Dad Poor Dad", "Robert T. Kiyosaki", "Finance", "978-1612680194", "RACK-FN01", 3, 14.0, 3},
    {"The Psychology of Money", "Morgan Housel", "Finance", "978-0857197689", "RACK-FN02", 3, 14.7, 3},
    {"Deep Work", "Cal Newport", "Self-Growth", "978-1455586691", "RACK-GN02", 3, 14.3, 3},
    {"The 7 Habits of Highly Effective People", "Stephen R. Covey", "Self-Growth", "978-1982137274", "RACK-GN03", 2, 9.4, 2},
    {"Thinking, Fast and Slow", "Daniel Kahneman", "Psychology", "978-0374533557", "RACK-PS01", 2, 9.5, 2},
    {"1984", "George Orwell", "Fiction", "978-0451524935", "RACK-FC01", 3, 14.6, 3},
    {"The Alchemist", "Paulo Coelho", "Fiction", "978-0062315007", "RACK-FC02", 3, 14.2, 3},
    {"To Kill a Mockingbird", "Harper Lee", "Fiction", "978-0060935467", "RACK-FC03", 2, 9.6, 2},
    {"Dune", "Frank Herbert", "Fiction", "978-0441172719", "RACK-FC04", 2, 9.5, 2},
    {"Environmental Studies & Sustainability", "Erach Bharucha", "Environment", "978-8173715402", "RACK-EV01", 3, 13.8, 3},
};

void populateDefaultBooksInMemory(void) {
    runtimeBookCount = 0;
    for (int i = 0; i < INITIAL_BOOK_COUNT; i++) {
        Book b;
        memset(&b, 0, sizeof(Book));
        b.bookId = 1001 + i;
        strncpy(b.title, INITIAL_50_BOOKS[i].title, sizeof(b.title) - 1);
        strncpy(b.author, INITIAL_50_BOOKS[i].author, sizeof(b.author) - 1);
        strncpy(b.category, INITIAL_50_BOOKS[i].category, sizeof(b.category) - 1);
        strncpy(b.isbn, INITIAL_50_BOOKS[i].isbn, sizeof(b.isbn) - 1);
        strncpy(b.shelfCode, INITIAL_50_BOOKS[i].shelfCode, sizeof(b.shelfCode) - 1);
        b.totalCopies = INITIAL_50_BOOKS[i].copies;
        b.availableCopies = INITIAL_50_BOOKS[i].copies;
        b.ratingSum = INITIAL_50_BOOKS[i].ratingSum;
        b.ratingCount = INITIAL_50_BOOKS[i].ratingCount;
        b.isActive = 1;
        runtimeBooks[runtimeBookCount++] = b;
    }
}

void syncBooksToDisk(void) {
    FILE *fp = fopen(BOOKS_FILE, "wb");
    if (fp != NULL) {
        if (runtimeBookCount > 0) {
            fwrite(runtimeBooks, sizeof(Book), (size_t)runtimeBookCount, fp);
        }
        fclose(fp);
    }
}

void seedStarterCatalogIfEmpty(void) {
    FILE *fp = fopen(BOOKS_FILE, "rb");
    if (fp != NULL) {
        Book diskList[200];
        size_t readN = fread(diskList, sizeof(Book), 200, fp);
        fclose(fp);
        if (readN >= (size_t)INITIAL_BOOK_COUNT) {
            runtimeBookCount = (int)readN;
            for (size_t i = 0; i < readN; i++) {
                runtimeBooks[i] = diskList[i];
            }
            return;
        }
    }
    populateDefaultBooksInMemory();
    syncBooksToDisk();
}

void viewAllBooks(void) {
    if (runtimeBookCount == 0) {
        populateDefaultBooksInMemory();
    }
    int count = 0;
    int totalStock = 0;
    int availStock = 0;

    printf("\n==============================================================================================================\n");
    printf("%-4s | %-6s | %-32s | %-22s | %-12s | %-9s | %-7s | %s\n",
           "S.No", "ID", "Book Title", "Author", "Category", "Shelf", "Avail", "Rating");
    printf("--------------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < runtimeBookCount; i++) {
        Book *b = &runtimeBooks[i];
        if (!b->isActive) continue;
        count++;
        totalStock += b->totalCopies;
        availStock += b->availableCopies;
        double avgRating = (b->ratingCount > 0) ? (b->ratingSum / b->ratingCount) : 0.0;
        printf("%-4d | %-6d | %-32.32s | %-22.22s | %-12.12s | %-9s | %2d/%-4d | %.1f/5 (%d)\n",
               count, b->bookId, b->title, b->author, b->category, b->shelfCode,
               b->availableCopies, b->totalCopies, avgRating, b->ratingCount);
    }

    printf("==============================================================================================================\n");
    printf("Total Active Titles: %d | Total Copies: %d | Available Copies: %d\n",
           count, totalStock, availStock);
}

/* -------------------- Catalog & Book Features -------------------- */

void searchBooksSmart(void) {
    char keyword[64];
    printf("\n=== Smart Book Search (Title / Author / Category / ISBN) ===\n");
    printf("Enter search keyword: ");
    clearInputBuffer();
    if (!fgets(keyword, sizeof(keyword), stdin)) return;
    keyword[strcspn(keyword, "\n")] = '\0';

    FILE *fp = fopen(BOOKS_FILE, "rb");
    if (!fp) {
        printf("No books database found.\n");
        return;
    }

    Book b;
    int matches = 0;
    printf("\n%-6s | %-30s | %-22s | %-12s | %-9s | %s\n",
           "ID", "Title", "Author", "Category", "Shelf", "Status");
    printf("-------------------------------------------------------------------------------------------------\n");
    while (fread(&b, sizeof(Book), 1, fp) == 1) {
        if (!b.isActive) continue;
        if (containsCaseInsensitive(b.title, keyword) ||
            containsCaseInsensitive(b.author, keyword) ||
            containsCaseInsensitive(b.category, keyword) ||
            containsCaseInsensitive(b.isbn, keyword)) {
            printf("%-6d | %-30.30s | %-22.22s | %-12.12s | %-9s | %d/%d Available\n",
                   b.bookId, b.title, b.author, b.category, b.shelfCode,
                   b.availableCopies, b.totalCopies);
            matches++;
        }
    }
    fclose(fp);
    if (matches == 0) {
        printf("No matching books found for '%s'.\n", keyword);
    } else {
        printf("Found %d matching title(s).\n", matches);
    }
}

void viewTrendingBooks(void) {
    FILE *fp = fopen(BOOKS_FILE, "rb");
    if (!fp) return;
    Book list[200];
    int n = 0;
    while (n < 200 && fread(&list[n], sizeof(Book), 1, fp) == 1) {
        if (list[n].isActive) n++;
    }
    fclose(fp);

    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (list[j].timesBorrowed < list[j + 1].timesBorrowed) {
                Book tmp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = tmp;
            }
        }
    }

    printf("\n=== Top Trending & Most Borrowed Books ===\n");
    int limit = (n < 5) ? n : 5;
    for (int i = 0; i < limit; i++) {
        double avg = (list[i].ratingCount > 0) ? (list[i].ratingSum / list[i].ratingCount) : 0.0;
        printf("#%d [ID:%d] %s by %s | Borrowed: %d times | Rating: %.1f/5\n",
               i + 1, list[i].bookId, list[i].title, list[i].author,
               list[i].timesBorrowed, avg);
    }
}

/* -------------------- Member Registration & Portal -------------------- */

void registerMember(void) {
    Member m;
    memset(&m, 0, sizeof(Member));

    printf("\n=== New Library Member Registration ===\n");
    printf("Enter Full Name: ");
    clearInputBuffer();
    if (!fgets(m.name, sizeof(m.name), stdin)) return;
    m.name[strcspn(m.name, "\n")] = '\0';
    if (strlen(m.name) == 0) {
        printf("Name cannot be empty.\n");
        return;
    }

    int roleOpt = 0;
    printf("Select Role (1. Student [Max 3 books, 14 days] | 2. Faculty [Max 6 books, 30 days]): ");
    if (scanf("%d", &roleOpt) != 1 || (roleOpt != 1 && roleOpt != 2)) {
        printf("Invalid role.\n");
        clearInputBuffer();
        return;
    }
    strcpy(m.role, (roleOpt == 1) ? "Student" : "Faculty");

    printf("Enter Department (e.g., CSE, ECE, Mechanical): ");
    clearInputBuffer();
    if (!fgets(m.department, sizeof(m.department), stdin)) return;
    m.department[strcspn(m.department, "\n")] = '\0';

    printf("Enter Contact Number: ");
    if (!fgets(m.contact, sizeof(m.contact), stdin)) return;
    m.contact[strcspn(m.contact, "\n")] = '\0';

    int pin1 = 0, pin2 = 0;
    printf("Create 4-digit Member PIN (1000-9999): ");
    if (scanf("%d", &pin1) != 1 || pin1 < 1000 || pin1 > 9999) {
        printf("Invalid PIN.\n");
        clearInputBuffer();
        return;
    }
    printf("Confirm PIN: ");
    if (scanf("%d", &pin2) != 1 || pin1 != pin2) {
        printf("PIN mismatch.\n");
        clearInputBuffer();
        return;
    }

    m.memberId = nextMemberId();
    m.pin = pin1;
    m.isActive = 1;

    if (appendMember(&m)) {
        printf("\nLibrary Card Generated Successfully!\n");
        printf("Member ID  : %d\n", m.memberId);
        printf("Name       : %s (%s - %s)\n", m.name, m.role, m.department);
        printf("Borrow Cap : %d books for %d days each\n",
               getMaxAllowedBooks(&m), getAllowedLoanDays(&m));
    } else {
        printf("Error saving member profile.\n");
    }
}

void issueBookToMember(Member *m, long memberOffset) {
    if (m->isBlocked) {
        printf("Your library card is BLOCKED. Contact Librarian.\n");
        return;
    }
    if (m->pendingFines > 0.0) {
        printf("Please clear pending overdue fine of Rs.%.2f before borrowing.\n", m->pendingFines);
        return;
    }
    if (m->activeLoansCount >= getMaxAllowedBooks(m)) {
        printf("Borrowing limit reached (%d/%d books active).\n",
               m->activeLoansCount, getMaxAllowedBooks(m));
        return;
    }

    int bookId = 0;
    printf("\nEnter Book ID to Borrow: ");
    if (scanf("%d", &bookId) != 1) {
        clearInputBuffer();
        return;
    }

    Book b;
    long bookOffset = 0;
    if (!findBookById(bookId, &b, &bookOffset)) {
        printf("Book ID #%d not found.\n", bookId);
        return;
    }

    /* Check if member already holds this exact book */
    FILE *lfp = fopen(LOANS_FILE, "rb");
    if (lfp) {
        LoanRecord chk;
        while (fread(&chk, sizeof(LoanRecord), 1, lfp) == 1) {
            if (!chk.isReturned && chk.memberId == m->memberId && chk.bookId == bookId) {
                printf("You already have an active copy of '%s'.\n", b.title);
                fclose(lfp);
                return;
            }
        }
        fclose(lfp);
    }

    if (b.availableCopies <= 0) {
        printf("All copies of '%s' are currently issued out.\n", b.title);
        printf("Join Waitlist Queue? (1. Yes | 2. No): ");
        int wChoice = 0;
        if (scanf("%d", &wChoice) == 1 && wChoice == 1) {
            for (int i = 0; i < b.waitlistCount; i++) {
                if (b.waitlistMemberIds[i] == m->memberId) {
                    printf("You are already at position #%d in the waitlist.\n", i + 1);
                    return;
                }
            }
            if (b.waitlistCount < MAX_WAITLIST) {
                b.waitlistMemberIds[b.waitlistCount++] = m->memberId;
                updateBookAtOffset(&b, bookOffset);
                printf("Added to waitlist! Your Queue Position: #%d\n", b.waitlistCount);
            } else {
                printf("Waitlist is full.\n");
            }
        }
        return;
    }

    /* If waitlist exists, ensure member is first in queue */
    if (b.waitlistCount > 0) {
        if (b.waitlistMemberIds[0] != m->memberId) {
            printf("Book is reserved for Waitlist Member #%d.\n", b.waitlistMemberIds[0]);
            return;
        }
        for (int i = 1; i < b.waitlistCount; i++) {
            b.waitlistMemberIds[i - 1] = b.waitlistMemberIds[i];
        }
        b.waitlistCount--;
    }

    time_t now = time(NULL);
    int days = getAllowedLoanDays(m);
    time_t due = now + (time_t)days * 86400;

    LoanRecord lr;
    memset(&lr, 0, sizeof(LoanRecord));
    lr.loanId = nextLoanId();
    lr.bookId = b.bookId;
    lr.memberId = m->memberId;
    lr.issueEpoch = now;
    lr.dueEpoch = due;
    formatEpochDate(now, lr.issueDateStr, sizeof(lr.issueDateStr));
    formatEpochDate(due, lr.dueDateStr, sizeof(lr.dueDateStr));
    strcpy(lr.returnDateStr, "ACTIVE");

    b.availableCopies--;
    b.timesBorrowed++;
    m->activeLoansCount++;

    appendLoanRecord(&lr);
    updateBookAtOffset(&b, bookOffset);
    updateMemberAtOffset(m, memberOffset);

    printf("\nBook Issued Successfully!\n");
    printf("Loan ID    : %d\n", lr.loanId);
    printf("Book       : %s (Shelf: %s)\n", b.title, b.shelfCode);
    printf("Issue Date : %s\n", lr.issueDateStr);
    printf("Due Date   : %s (%d days)\n", lr.dueDateStr, days);
}

void viewMyLoans(const Member *m) {
    FILE *fp = fopen(LOANS_FILE, "rb");
    if (!fp) {
        printf("No loan history found.\n");
        return;
    }
    LoanRecord lr;
    time_t now = time(NULL);
    int count = 0;

    printf("\n=== My Borrowed Books & Loan History ===\n");
    printf("%-7s | %-6s | %-26s | %-16s | %-16s | %-8s | %s\n",
           "LoanID", "BookID", "Title", "Issued", "Due Date", "Fine", "Status");
    printf("------------------------------------------------------------------------------------------------------------\n");
    while (fread(&lr, sizeof(LoanRecord), 1, fp) == 1) {
        if (lr.memberId != m->memberId) continue;
        Book b;
        char titleBuf[32] = "Unknown Book";
        if (findBookById(lr.bookId, &b, NULL)) {
            strncpy(titleBuf, b.title, sizeof(titleBuf) - 1);
        }
        double liveFine = lr.isReturned ? lr.fineCharged : calculateOverdueFine(lr.dueEpoch, now);
        printf("%-7d | %-6d | %-26.26s | %-16s | %-16s | Rs.%-5.0f | %s\n",
               lr.loanId, lr.bookId, titleBuf, lr.issueDateStr, lr.dueDateStr,
               liveFine, lr.isReturned ? "RETURNED" : "ISSUED");
        count++;
    }
    printf("------------------------------------------------------------------------------------------------------------\n");
    if (count == 0) printf("No books borrowed yet.\n");
    fclose(fp);
}

void renewBookLoan(const Member *m) {
    viewMyLoans(m);
    int loanId = 0;
    printf("Enter Active Loan ID to Renew (+7 days, max 1 renewal): ");
    if (scanf("%d", &loanId) != 1) {
        clearInputBuffer();
        return;
    }

    FILE *fp = fopen(LOANS_FILE, "rb");
    if (!fp) return;
    LoanRecord lr;
    long offset = 0;
    int found = 0;
    while (fread(&lr, sizeof(LoanRecord), 1, fp) == 1) {
        if (lr.loanId == loanId && lr.memberId == m->memberId && !lr.isReturned) {
            found = 1;
            break;
        }
        offset = ftell(fp);
    }
    fclose(fp);

    if (!found) {
        printf("Active Loan ID not found.\n");
        return;
    }
    if (lr.renewalsUsed >= 1) {
        printf("Maximum renewals (1) already used for this loan.\n");
        return;
    }

    Book b;
    if (findBookById(lr.bookId, &b, NULL) && b.waitlistCount > 0) {
        printf("Cannot renew: another member is waiting in the reservation queue.\n");
        return;
    }

    time_t now = time(NULL);
    if (now > lr.dueEpoch) {
        printf("Cannot renew an overdue book. Please return it first.\n");
        return;
    }

    lr.dueEpoch += 7 * 86400;
    lr.renewalsUsed++;
    formatEpochDate(lr.dueEpoch, lr.dueDateStr, sizeof(lr.dueDateStr));
    updateLoanAtOffset(&lr, offset);
    printf("Loan #%d renewed! New Due Date: %s\n", lr.loanId, lr.dueDateStr);
}

void returnBookAndRate(Member *m, long memberOffset) {
    viewMyLoans(m);
    int loanId = 0;
    printf("Enter Loan ID to Return: ");
    if (scanf("%d", &loanId) != 1) {
        clearInputBuffer();
        return;
    }

    FILE *fp = fopen(LOANS_FILE, "rb");
    if (!fp) return;
    LoanRecord lr;
    long loanOffset = 0;
    int found = 0;
    while (fread(&lr, sizeof(LoanRecord), 1, fp) == 1) {
        if (lr.loanId == loanId && lr.memberId == m->memberId && !lr.isReturned) {
            found = 1;
            break;
        }
        loanOffset = ftell(fp);
    }
    fclose(fp);

    if (!found) {
        printf("Active Loan ID not found.\n");
        return;
    }

    time_t now = time(NULL);
    double fine = calculateOverdueFine(lr.dueEpoch, now);

    lr.isReturned = 1;
    lr.returnEpoch = now;
    lr.fineCharged = fine;
    formatEpochDate(now, lr.returnDateStr, sizeof(lr.returnDateStr));

    Book b;
    long bookOffset = 0;
    if (findBookById(lr.bookId, &b, &bookOffset)) {
        b.availableCopies++;
        int rating = 0;
        printf("Rate '%s' (1-5 stars, or 0 to skip): ", b.title);
        if (scanf("%d", &rating) == 1 && rating >= 1 && rating <= 5) {
            b.ratingSum += rating;
            b.ratingCount++;
        }
        updateBookAtOffset(&b, bookOffset);
        if (b.waitlistCount > 0) {
            printf("ALERT: Book reserved for next Waitlist Member #%d!\n", b.waitlistMemberIds[0]);
        }
    }

    if (m->activeLoansCount > 0) m->activeLoansCount--;
    m->pendingFines += fine;

    updateLoanAtOffset(&lr, loanOffset);
    updateMemberAtOffset(m, memberOffset);

    printf("Book Returned Successfully!\n");
    if (fine > 0) {
        printf("Overdue Fine Added: Rs.%.2f (Total Pending Fine: Rs.%.2f)\n", fine, m->pendingFines);
    }
}

void payLibraryFines(Member *m, long memberOffset) {
    printf("\n=== Library Fine Counter ===\n");
    printf("Pending Fine Balance: Rs.%.2f | Lifetime Paid: Rs.%.2f\n",
           m->pendingFines, m->totalFinesPaid);
    if (m->pendingFines <= 0.0) {
        printf("Zero pending fines!\n");
        return;
    }
    double pay = 0.0;
    printf("Enter amount to pay: Rs.");
    if (scanf("%lf", &pay) != 1 || pay <= 0 || pay > m->pendingFines) {
        printf("Enter a valid amount up to Rs.%.2f.\n", m->pendingFines);
        clearInputBuffer();
        return;
    }
    m->pendingFines -= pay;
    m->totalFinesPaid += pay;
    updateMemberAtOffset(m, memberOffset);
    printf("Payment of Rs.%.2f received! Remaining Fine: Rs.%.2f\n", pay, m->pendingFines);
}

void memberPortal(void) {
    int memberId = 0, pin = 0;
    Member m;
    long offset = 0;

    printf("\n=== Student / Faculty Member Login ===\n");
    printf("Enter Member ID: ");
    if (scanf("%d", &memberId) != 1) { clearInputBuffer(); return; }
    if (!findMemberById(memberId, &m, &offset)) {
        printf("Member ID not found.\n");
        return;
    }
    printf("Enter 4-digit PIN: ");
    if (scanf("%d", &pin) != 1 || pin != m.pin) {
        printf("Invalid PIN.\n");
        clearInputBuffer();
        return;
    }

    int ch = 0;
    while (1) {
        printf("\n======================================================\n");
        printf("  MEMBER PORTAL: %s (ID #%d | %s - %s)\n", m.name, m.memberId, m.role, m.department);
        printf("  Active Books: %d/%d | Pending Fines: Rs.%.2f\n",
               m.activeLoansCount, getMaxAllowedBooks(&m), m.pendingFines);
        printf("======================================================\n");
        printf("  1. Browse Full Library Catalog\n");
        printf("  2. Smart Search Books (Title / Author / Category / ISBN)\n");
        printf("  3. Top Trending & Highest-Rated Books\n");
        printf("  4. Issue / Borrow a Book (or Join Waitlist)\n");
        printf("  5. Renew Borrowed Book (+7 Days)\n");
        printf("  6. Return Book & Submit Rating\n");
        printf("  7. My Borrowed Books & Loan History\n");
        printf("  8. Pay Overdue Fines\n");
        printf("  9. Logout\n");
        printf("Choose (1-9): ");

        if (scanf("%d", &ch) != 1) { clearInputBuffer(); continue; }
        switch (ch) {
            case 1: viewAllBooks(); break;
            case 2: searchBooksSmart(); break;
            case 3: viewTrendingBooks(); break;
            case 4: issueBookToMember(&m, offset); break;
            case 5: renewBookLoan(&m); break;
            case 6: returnBookAndRate(&m, offset); break;
            case 7: viewMyLoans(&m); break;
            case 8: payLibraryFines(&m, offset); break;
            case 9: return;
            default: printf("Invalid option.\n");
        }
    }
}

/* -------------------- Librarian / Admin Desk -------------------- */

void addNewBookAdmin(void) {
    Book b;
    memset(&b, 0, sizeof(Book));

    printf("\n=== Add New Book to Catalog ===\n");
    clearInputBuffer();
    printf("Book Title   : ");
    if (!fgets(b.title, sizeof(b.title), stdin)) return;
    b.title[strcspn(b.title, "\n")] = '\0';

    printf("Author Name  : ");
    if (!fgets(b.author, sizeof(b.author), stdin)) return;
    b.author[strcspn(b.author, "\n")] = '\0';

    printf("Category     : ");
    if (!fgets(b.category, sizeof(b.category), stdin)) return;
    b.category[strcspn(b.category, "\n")] = '\0';

    printf("ISBN Code    : ");
    if (!fgets(b.isbn, sizeof(b.isbn), stdin)) return;
    b.isbn[strcspn(b.isbn, "\n")] = '\0';

    printf("Shelf Code   : ");
    if (!fgets(b.shelfCode, sizeof(b.shelfCode), stdin)) return;
    b.shelfCode[strcspn(b.shelfCode, "\n")] = '\0';

    printf("Total Copies : ");
    if (scanf("%d", &b.totalCopies) != 1 || b.totalCopies <= 0) {
        printf("Invalid copy count.\n");
        clearInputBuffer();
        return;
    }

    b.bookId = nextBookId();
    b.availableCopies = b.totalCopies;
    b.isActive = 1;

    if (runtimeBookCount < 200) { runtimeBooks[runtimeBookCount++] = b; syncBooksToDisk(); }
    printf("Book Added! Assigned Book ID: #%d (Shelf: %s)\n", b.bookId, b.shelfCode);
}

void restockBookCopies(void) {
    int bookId = 0, extra = 0;
    Book b;
    long offset = 0;
    printf("Enter Book ID to add copies: ");
    if (scanf("%d", &bookId) != 1) { clearInputBuffer(); return; }
    if (!findBookById(bookId, &b, &offset)) {
        printf("Book not found.\n");
        return;
    }
    printf("Current Stock of '%s': %d/%d. Add how many copies? ",
           b.title, b.availableCopies, b.totalCopies);
    if (scanf("%d", &extra) != 1 || extra <= 0) { clearInputBuffer(); return; }
    b.totalCopies += extra;
    b.availableCopies += extra;
    updateBookAtOffset(&b, offset);
    printf("Updated Stock: %d/%d copies.\n", b.availableCopies, b.totalCopies);
}

void viewAllMembersAdmin(void) {
    FILE *fp = fopen(MEMBERS_FILE, "rb");
    if (!fp) {
        printf("No members registered yet.\n");
        return;
    }
    Member m;
    printf("\n%-6s | %-22s | %-8s | %-12s | %-7s | %-9s | %s\n",
           "ID", "Name", "Role", "Dept", "Issued", "Fine", "Status");
    printf("------------------------------------------------------------------------------------\n");
    while (fread(&m, sizeof(Member), 1, fp) == 1) {
        if (!m.isActive) continue;
        printf("%-6d | %-22.22s | %-8s | %-12.12s | %d/%-5d | Rs.%-6.0f | %s\n",
               m.memberId, m.name, m.role, m.department,
               m.activeLoansCount, getMaxAllowedBooks(&m), m.pendingFines,
               m.isBlocked ? "BLOCKED" : "ACTIVE");
    }
    fclose(fp);
}

void toggleMemberBlockAdmin(void) {
    int memberId = 0;
    Member m;
    long offset = 0;
    printf("Enter Member ID to Block/Unblock: ");
    if (scanf("%d", &memberId) != 1) { clearInputBuffer(); return; }
    if (!findMemberById(memberId, &m, &offset)) {
        printf("Member not found.\n");
        return;
    }
    m.isBlocked = !m.isBlocked;
    updateMemberAtOffset(&m, offset);
    printf("Member #%d (%s) is now %s.\n",
           m.memberId, m.name, m.isBlocked ? "BLOCKED" : "UNBLOCKED");
}

void exportLibraryAuditReport(void) {
    FILE *out = fopen("Library_Audit_Report.txt", "w");
    if (!out) {
        printf("Could not create report file.\n");
        return;
    }
    time_t now = time(NULL);
    char dateBuf[32];
    formatEpochDate(now, dateBuf, sizeof(dateBuf));

    fprintf(out, "UNIVERSITY CENTRAL LIBRARY - INVENTORY & CIRCULATION REPORT\n");
    fprintf(out, "Generated On: %s\n\n", dateBuf);

    FILE *bfp = fopen(BOOKS_FILE, "rb");
    int totalTitles = 0, totalCopies = 0, availCopies = 0;
    if (bfp) {
        Book b;
        fprintf(out, "--- BOOK CATALOG ---\n");
        while (fread(&b, sizeof(Book), 1, bfp) == 1) {
            if (!b.isActive) continue;
            totalTitles++;
            totalCopies += b.totalCopies;
            availCopies += b.availableCopies;
            fprintf(out, "[#%d] %s | %s | Shelf: %s | Stock: %d/%d | Borrows: %d\n",
                    b.bookId, b.title, b.author, b.shelfCode,
                    b.availableCopies, b.totalCopies, b.timesBorrowed);
        }
        fclose(bfp);
    }

    fprintf(out, "\nSummary: %d Titles | %d Total Copies | %d Available | %d Issued Out\n",
            totalTitles, totalCopies, availCopies, totalCopies - availCopies);
    fclose(out);
    printf("Audit Report exported to 'Library_Audit_Report.txt'!\n");
}

void librarianAdminConsole(void) {
    int code = 0;
    printf("\n=== LIBRARIAN ADMIN DESK ===\n");
    printf("Enter Librarian Passcode (Default: 8888): ");
    if (scanf("%d", &code) != 1 || code != LIBRARIAN_PASSCODE) {
        printf("Access Denied.\n");
        clearInputBuffer();
        return;
    }

    int ch = 0;
    while (1) {
        printf("\n--- Librarian Control Panel ---\n");
        printf("1. View Full Book Catalog\n");
        printf("2. Add New Book Title\n");
        printf("3. Restock Book Copies\n");
        printf("4. View All Registered Members\n");
        printf("5. Block / Unblock Member Card\n");
        printf("6. Export Library Inventory & Audit Report (.txt)\n");
        printf("7. Exit Admin Panel\n");
        printf("Select: ");
        if (scanf("%d", &ch) != 1) { clearInputBuffer(); continue; }

        switch (ch) {
            case 1: viewAllBooks(); break;
            case 2: addNewBookAdmin(); break;
            case 3: restockBookCopies(); break;
            case 4: viewAllMembersAdmin(); break;
            case 5: toggleMemberBlockAdmin(); break;
            case 6: exportLibraryAuditReport(); break;
            case 7: return;
            default: printf("Invalid choice.\n");
        }
    }
}

/* -------------------- Main Entry -------------------- */

int main(void) {
    seedStarterCatalogIfEmpty();
    int choice = 0;

    while (1) {
        printf("\n======================================================\n");
        printf("        SMART UNIVERSITY LIBRARY MANAGEMENT SYSTEM    \n");
        printf("======================================================\n");
        printf("  1. Browse Library Catalog (Public Kiosk)\n");
        printf("  2. Smart Search Books (Title / Author / Category / ISBN)\n");
        printf("  3. Top Trending & Highest-Rated Books\n");
        printf("  4. New Member Registration (Student / Faculty)\n");
        printf("  5. Member Login (Borrow / Return / Renew / Fines)\n");
        printf("  6. Librarian Admin Console\n");
        printf("  7. Exit System\n");
        printf("Select Option (1-7): ");

        if (scanf("%d", &choice) != 1) {
            printf("Please enter a valid number.\n");
            clearInputBuffer();
            continue;
        }

        switch (choice) {
            case 1: viewAllBooks(); break;
            case 2: searchBooksSmart(); break;
            case 3: viewTrendingBooks(); break;
            case 4: registerMember(); break;
            case 5: memberPortal(); break;
            case 6: librarianAdminConsole(); break;
            case 7:
                printf("Library System Closed. Goodbye!\n");
                return 0;
            default:
                printf("Invalid option. Choose 1-7.\n");
        }
    }
}
