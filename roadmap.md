# Project: School Management System – C GUI Desktop Application

Act as a Senior C Software Engineer, Desktop UI/UX Designer, Software Architect, and Database Engineer.

Build a complete, modern, professional, GUI-based School Management System desktop application using pure C.

## 1. Technology Stack

- Programming Language: C (C11)
- GUI Framework: GTK4
- Database: SQLite3
- Build System: CMake
- Styling: GTK CSS
- Platform: Windows 10/11
- Architecture: Modular architecture, clean separation of concerns
- Optional Charts: Cairo or GTK-compatible chart library

Do not use C++.
All core logic must be written in C.

## 2. UI/UX Design

Create a premium modern school administration dashboard.

Design inspiration:
- Professional SaaS admin dashboards
- Clean typography
- Modern card-based layout
- Light theme by default
- Dark mode toggle
- Responsive layout within desktop window resizing
- Rounded corners
- Subtle shadows
- Consistent spacing
- Professional icons
- Smooth and subtle transitions where GTK supports them

Main application layout:

- Left sidebar navigation
- Top header/navbar
- Main content area
- Search bar
- Notification icon
- User profile section
- Breadcrumb navigation
- Status bar

Window should support resizing and minimum dimensions.

## 3. Dashboard

Create dashboard widgets:

- Total Students
- Total Teachers
- Total Staff
- Total Classes
- Today's Attendance
- Fee Collection This Month
- Pending Fees
- New Admissions

Dashboard sections:

- Attendance Overview Chart
- Fee Collection Chart
- Recent Admissions Table
- Recent Activities
- Upcoming Examinations
- Quick Actions

Quick action buttons:

- Add Student
- Collect Fees
- Mark Attendance
- Add Teacher
- Generate Report

All dashboard statistics must be fetched from SQLite database dynamically.

## 4. Student Management

Implement complete CRUD functionality:

- Add Student
- Edit Student
- View Student Profile
- Delete Student
- Search Student
- Filter by Class
- Filter by Section
- Filter by Gender
- Filter by Admission Year
- Student photo
- Student ID / Admission Number

Student fields:

- Student ID
- Admission Number
- Full Name
- Father's Name
- Mother's Name
- Date of Birth
- Gender
- Category
- Phone
- Email
- Address
- Admission Date
- Class
- Section
- Roll Number
- Previous School
- Emergency Contact
- Blood Group
- Student Photo
- Status

Use a paginated student table with sorting and filtering.

## 5. Teacher & Staff Management

Implement:

- Add Teacher
- Edit Teacher
- Delete Teacher
- Teacher Profile
- Search Teacher
- Teacher ID
- Name
- Phone
- Email
- Address
- Qualification
- Subject
- Joining Date
- Salary
- Employment Status

## 6. Class & Section Management

Features:

- Create Classes
- Edit Classes
- Delete Classes
- Create Sections
- Assign Class Teachers
- Assign Subjects
- Student Capacity
- Academic Year Management

## 7. Attendance Management

Implement:

- Daily attendance
- Student-wise attendance
- Class-wise attendance
- Present
- Absent
- Late
- Half-day
- Date-based attendance
- Attendance editing with permission
- Attendance percentage calculation
- Monthly attendance report

Add bulk attendance marking interface.

## 8. Fees Management

Create a complete fee management module.

Features:

- Fee Structure
- Class-wise Fee
- Monthly Fees
- Annual Fees
- Admission Fees
- Transport Fees
- Other Charges
- Student Fee Ledger
- Payment Entry
- Partial Payments
- Discounts
- Late Fine
- Due Fees
- Pending Fees
- Payment History
- Receipt Generation
- Printable receipts

Payment methods:

- Cash
- UPI
- Bank Transfer
- Cheque

Each payment should have a unique receipt number.

Never allow duplicate payment records due to accidental repeated submission.

## 9. Examination & Results

Implement:

- Create Examination
- Examination Schedule
- Subjects
- Maximum Marks
- Obtained Marks
- Grade Calculation
- Percentage
- Rank
- Pass/Fail
- Student Report Card
- Class Results
- Printable Marksheet

Allow configurable grading rules.

## 10. Timetable

Features:

- Class timetable
- Teacher timetable
- Subject allocation
- Period management
- Room allocation
- Day-wise schedule
- Conflict detection

Prevent a teacher from being assigned to two classes at the same time.

## 11. Reports

Create report generation modules:

- Student List
- Class-wise Student List
- Attendance Report
- Fee Collection Report
- Pending Fee Report
- Teacher Report
- Examination Report
- Result Report
- Admission Report

Export options:

- CSV
- Printable PDF if a suitable C-compatible library is available

## 12. Database Design

Use SQLite.

Create separate tables for:

- users
- roles
- students
- teachers
- staff
- classes
- sections
- subjects
- academic_years
- attendance
- fee_structures
- fee_payments
- examinations
- exam_subjects
- exam_results
- timetables
- notifications
- activity_logs
- settings

Requirements:

- Foreign keys
- Indexes
- Prepared statements
- Transactions
- Unique constraints
- Database migration/version system
- Database backup and restore
- Automatic database initialization

## 13. Authentication & Security

Create:

- Login screen
- Username and password
- Password hashing using a secure cryptographic library
- Admin role
- Teacher role
- Accountant role
- Staff role
- Permission-based access
- Session/logout handling
- Login attempt protection
- Audit logs

Never store plaintext passwords.

## 14. UI Components

Create reusable C GTK4 components:

- Sidebar
- Header
- Dashboard Cards
- Data Tables
- Search Field
- Input Forms
- Dropdowns
- Date Picker
- Dialogs
- Toast Notifications
- Confirmation Dialogs
- Pagination
- Empty State
- Loading State
- Error State

Use reusable components and avoid duplicating UI code.

## 15. Application Settings

Settings page:

- School Name
- School Logo
- School Address
- Phone
- Email
- Website
- Principal Name
- Academic Year
- Currency
- Theme
- Database Backup
- Restore Database

## 16. Printing

Implement print-friendly layouts for:

- Fee Receipt
- Student ID Card
- Report Card
- Attendance Report
- Student Details
- Examination Marksheet

Support standard A4 paper and receipt-size printing where appropriate.

## 17. Project Folder Structure

Use a clean structure:

school-management/
├── CMakeLists.txt
├── README.md
├── assets/
│   ├── icons/
│   ├── images/
│   └── styles/
│       └── main.css
├── include/
│   ├── app.h
│   ├── database.h
│   ├── student.h
│   ├── teacher.h
│   ├── attendance.h
│   ├── fees.h
│   ├── dashboard.h
│   └── ui.h
├── src/
│   ├── main.c
│   ├── app.c
│   ├── database.c
│   ├── auth/
│   ├── modules/
│   ├── ui/
│   ├── reports/
│   └── utils/
├── database/
├── docs/
└── tests/

## 18. Code Quality

- Write clean modular C code.
- Use appropriate structs and enums.
- Avoid giant single-file implementations.
- Use proper memory management.
- Prevent memory leaks.
- Handle SQLite errors properly.
- Validate all user inputs.
- Use prepared statements.
- Avoid unsafe functions.
- Include comments for complex logic.
- Implement logging.
- Build with compiler warnings enabled.
- Use consistent naming conventions.
- I would like one Git commit per file—for instance, if there are 5 files in the 'core' folder and 10 in the    'header' folder, there should be a total of 15 commits, each accompanied by a proper message explaining the reason for the change.

## 19. Development Workflow

Do not generate the entire project as one giant code dump.

Build the project incrementally in the following order:

Phase 1: Project Setup and GTK4 Application Shell  
Phase 2: Database and Migration System  
Phase 3: Login and Authentication  
Phase 4: Main Dashboard UI  
Phase 5: Student Management CRUD  
Phase 6: Teacher & Staff Management  
Phase 7: Class & Section Management  
Phase 8: Attendance System  
Phase 9: Fee Management  
Phase 10: Examination & Results  
Phase 11: Timetable  
Phase 12: Reports and Export  
Phase 13: Settings, Backup & Restore  
Phase 14: Testing and Bug Fixing  
Phase 15: Windows EXE Packaging and Installer

For every phase:

1. Explain what will be implemented.
2. Create the required files.
3. Provide complete source code for those files.
4. Explain how to compile and run.
5. Test for errors.
6. Do not break existing modules.
7. Maintain a working application at every phase.

## 20. Final Goal

Deliver a production-oriented Windows desktop School Management System with:

- Modern GUI
- Reliable SQLite database
- Secure login
- Complete CRUD modules
- Real dashboard statistics
- Attendance tracking
- Fee management
- Examination system
- Reports
- Printing
- Backup/restore
- Windows executable packaging

Start with Phase 1. Build the initial working GTK4 application shell, sidebar, top header, dashboard placeholder cards, navigation system, CSS styling, and CMake build configuration.

Do not stop until Phases end builds and runs successfully.