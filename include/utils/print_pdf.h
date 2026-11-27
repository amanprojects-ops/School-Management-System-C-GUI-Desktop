/**
 * @file print_pdf.h
 * @brief Shared vector PDF export (Cairo PDF + Pango) and native printing (GtkPrintOperation)
 *        for institutional reports, fee payment receipts, and student report cards.
 */

#ifndef SCHOOLCMS_PRINT_PDF_H
#define SCHOOLCMS_PRINT_PDF_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stddef.h>
#include "reports.h"
#include "fees.h"
#include "exams.h"

/**
 * @brief Completion status for synchronous print operations.
 */
typedef enum {
    PRINT_PDF_OK = 0,
    PRINT_PDF_CANCELLED,
    PRINT_PDF_ERROR
} PrintPdfStatus;

/**
 * @brief Async completion callback for Save As PDF file dialogs.
 * @param status          Operation result (OK, CANCELLED, or ERROR).
 * @param path_or_err     Saved filepath on PRINT_PDF_OK, error message on ERROR, or NULL.
 * @param user_data       Caller context pointer.
 */
typedef void (*PrintPdfResultCallback)(PrintPdfStatus status,
                                       const char *path_or_err,
                                       gpointer user_data);

/* ── Headless Vector PDF Exporters ────────────────────────────────── */

/**
 * @brief Export a generated ReportData table cleanly to a vector PDF file (multi-page A4).
 */
bool print_pdf_export_report(const ReportData *data, const char *filepath);

/**
 * @brief Export a fee payment receipt cleanly to an A4 vector PDF file.
 */
bool print_pdf_export_receipt(const FeePayment *payment, const char *filepath);

/**
 * @brief Export a student report card cleanly to an A4 vector PDF file.
 */
bool print_pdf_export_marksheet(const StudentReportCard *card, const char *filepath);

/* ── Interactive Native Windows Print Dialog Helpers ──────────────── */

/**
 * @brief Open native Windows Print Dialog for a tabular ReportData report.
 */
PrintPdfStatus print_pdf_print_report(GtkWidget *anchor_widget,
                                      const ReportData *data,
                                      char *err_buf,
                                      size_t err_size);

/**
 * @brief Open native Windows Print Dialog for a Fee Payment Receipt.
 */
PrintPdfStatus print_pdf_print_receipt(GtkWidget *anchor_widget,
                                       const FeePayment *payment,
                                       char *err_buf,
                                       size_t err_size);

/**
 * @brief Open native Windows Print Dialog for a Student Report Card.
 */
PrintPdfStatus print_pdf_print_marksheet(GtkWidget *anchor_widget,
                                         const StudentReportCard *card,
                                         char *err_buf,
                                         size_t err_size);

/* ── Interactive GTK4 File Save As PDF Dialog Helpers ─────────────── */

/**
 * @brief Prompt user with Save As dialog pre-filled in exports/ and save Report PDF.
 */
void print_pdf_prompt_export_report(GtkWidget *anchor_widget,
                                    const ReportData *data,
                                    PrintPdfResultCallback cb,
                                    gpointer user_data);

/**
 * @brief Prompt user with Save As dialog pre-filled in exports/ and save Receipt PDF.
 */
void print_pdf_prompt_export_receipt(GtkWidget *anchor_widget,
                                     const FeePayment *payment,
                                     PrintPdfResultCallback cb,
                                     gpointer user_data);

/**
 * @brief Prompt user with Save As dialog pre-filled in exports/ and save Marksheet PDF.
 */
void print_pdf_prompt_export_marksheet(GtkWidget *anchor_widget,
                                       const StudentReportCard *card,
                                       PrintPdfResultCallback cb,
                                       gpointer user_data);

#endif /* SCHOOLCMS_PRINT_PDF_H */
