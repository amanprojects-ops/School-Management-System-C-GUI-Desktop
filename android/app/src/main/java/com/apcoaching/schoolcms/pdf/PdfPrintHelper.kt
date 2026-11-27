package com.apcoaching.schoolcms.pdf

import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.graphics.pdf.PdfDocument
import android.os.Bundle
import android.os.CancellationSignal
import android.os.ParcelFileDescriptor
import android.print.PageRange
import android.print.PrintAttributes
import android.print.PrintDocumentAdapter
import android.print.PrintDocumentInfo
import android.print.PrintManager
import androidx.core.content.FileProvider
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

/**
 * Native Android Vector PDF generator & OS PrintManager adapter for
 * Fee Payment Receipts, Student Marksheets, and Analytics Reports.
 */
object PdfPrintHelper {

    private fun ensureExportDir(context: Context): File {
        val dir = File(context.filesDir, "exports")
        if (!dir.exists()) dir.mkdirs()
        return dir
    }

    private fun drawInstitutionHeader(
        canvas: android.graphics.Canvas,
        settings: JSONObject,
        docTitle: String,
        pageW: Int
    ): Float {
        val margin = 42f
        val schoolName = settings.optString("schoolName", "APCOACHING SchoolCMS")
        val address = settings.optString("schoolAddress", "")
        val phone = settings.optString("schoolPhone", "")
        val email = settings.optString("schoolEmail", "")

        val bannerPaint = Paint().apply { color = Color.parseColor("#1E3A8A") }
        canvas.drawRect(margin, 36f, pageW - margin, 118f, bannerPaint)

        val titlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            textSize = 18f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        }
        val subPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#E2E8F0")
            textSize = 10f
        }
        val tagPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#FDE047")
            textSize = 12f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        }

        canvas.drawText(schoolName, margin + 14f, 62f, titlePaint)
        canvas.drawText(address, margin + 14f, 80f, subPaint)
        canvas.drawText("Tel: $phone   |   $email", margin + 14f, 96f, subPaint)
        canvas.drawText(docTitle.uppercase(), margin + 14f, 112f, tagPaint)
        return 140f
    }

    fun generateFeeReceiptPdf(context: Context, settings: JSONObject, payment: JSONObject): File {
        val doc = PdfDocument()
        val pageInfo = PdfDocument.PageInfo.Builder(595, 842, 1).create()
        val page = doc.startPage(pageInfo)
        val c = page.canvas
        val margin = 42f

        var y = drawInstitutionHeader(c, settings, "Official Fee Payment Receipt", 595)
        val labelP = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#475569")
            textSize = 11f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        }
        val valP = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#0F172A")
            textSize = 11f
        }
        val lineP = Paint().apply {
            color = Color.parseColor("#CBD5E1")
            strokeWidth = 1f
        }
        val currency = settings.optString("currencySymbol", "₹")

        val rows = listOf(
            "Receipt Number" to payment.optString("receiptNo"),
            "Payment Date" to payment.optString("paymentDate"),
            "Student Name" to "${payment.optString("studentName")} (${payment.optString("admissionNo")})",
            "Class & Section" to "${payment.optString("className")} - ${payment.optString("sectionName")}",
            "Fee Head" to payment.optString("feeStructureName"),
            "Base Amount Paid" to "$currency ${"%.2f".format(payment.optDouble("amountPaid"))}",
            "Discount" to "$currency ${"%.2f".format(payment.optDouble("discount"))}",
            "Late Fine" to "$currency ${"%.2f".format(payment.optDouble("lateFine"))}",
            "Payment Mode" to "${payment.optString("paymentMethod")} ${payment.optString("transactionRef")}".trim(),
            "Collected By" to payment.optString("collectedByName")
        )

        rows.forEach { (k, v) ->
            c.drawText(k, margin + 8f, y, labelP)
            c.drawText(v, margin + 185f, y, valP)
            c.drawLine(margin, y + 8f, 595 - margin, y + 8f, lineP)
            y += 28f
        }

        y += 14f
        val totalBoxP = Paint().apply { color = Color.parseColor("#ECFDF5") }
        c.drawRect(margin, y, 595 - margin, y + 44f, totalBoxP)
        val totalP = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#065F46")
            textSize = 15f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        }
        c.drawText(
            "TOTAL PAID: $currency ${"%.2f".format(payment.optDouble("totalAmount"))}   [PAID & VERIFIED]",
            margin + 14f,
            y + 27f,
            totalP
        )

        doc.finishPage(page)
        val safeRcp = payment.optString("receiptNo", "receipt").replace(Regex("[^A-Za-z0-9_-]"), "_")
        val file = File(ensureExportDir(context), "Receipt_$safeRcp.pdf")
        FileOutputStream(file).use { doc.writeTo(it) }
        doc.close()
        return file
    }

    fun generateReportCardPdf(context: Context, settings: JSONObject, card: JSONObject): File {
        val doc = PdfDocument()
        val pageInfo = PdfDocument.PageInfo.Builder(595, 842, 1).create()
        val page = doc.startPage(pageInfo)
        val c = page.canvas
        val margin = 42f

        var y = drawInstitutionHeader(c, settings, "Official Student Report Card", 595)
        val boldP = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#0F172A")
            textSize = 11f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        }
        val txtP = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#1E293B")
            textSize = 11f
        }

        c.drawText("Exam: ${card.optString("examName")}", margin, y, boldP)
        y += 20f
        c.drawText(
            "Student: ${card.optString("studentName")} (${card.optString("admissionNo")})   Class: ${card.optString("className")} - ${card.optString("sectionName")}   Rank: #${card.optInt("classRank")}",
            margin,
            y,
            txtP
        )
        y += 26f

        val hdrBg = Paint().apply { color = Color.parseColor("#E2E8F0") }
        c.drawRect(margin, y - 14f, 595 - margin, y + 8f, hdrBg)
        c.drawText("Subject", margin + 8f, y, boldP)
        c.drawText("Marks", 320f, y, boldP)
        c.drawText("Max", 390f, y, boldP)
        c.drawText("Grade", 455f, y, boldP)
        c.drawText("Result", 505f, y, boldP)
        y += 26f

        val scores = card.optJSONArray("scores") ?: JSONArray()
        for (i in 0 until scores.length()) {
            val s = scores.getJSONObject(i)
            c.drawText(s.optString("subjectName").take(32), margin + 8f, y, txtP)
            c.drawText("%.1f".format(s.optDouble("marksObtained")), 320f, y, txtP)
            c.drawText("%.1f".format(s.optDouble("maxMarks")), 390f, y, txtP)
            c.drawText(s.optString("grade"), 455f, y, boldP)
            c.drawText(if (s.optBoolean("isPass")) "PASS" else "FAIL", 505f, y, txtP)
            y += 22f
            if (y > 760f) break
        }

        y += 16f
        c.drawText(
            "Grand Total: ${"%.1f".format(card.optDouble("totalObtained"))} / ${"%.1f".format(card.optDouble("totalMax"))}   |   Percentage: ${"%.2f".format(card.optDouble("percentage"))}%   |   Grade: ${card.optString("overallGrade")}",
            margin,
            y,
            boldP
        )

        doc.finishPage(page)
        val safeAdm = card.optString("admissionNo", "student").replace(Regex("[^A-Za-z0-9_-]"), "_")
        val file = File(ensureExportDir(context), "ReportCard_$safeAdm.pdf")
        FileOutputStream(file).use { doc.writeTo(it) }
        doc.close()
        return file
    }

    fun generateReportPdf(context: Context, settings: JSONObject, report: JSONObject): File {
        val doc = PdfDocument()
        val pageInfo = PdfDocument.PageInfo.Builder(842, 595, 1).create()
        val page = doc.startPage(pageInfo)
        val c = page.canvas
        val margin = 36f

        var y = drawInstitutionHeader(c, settings, report.optString("title", "Institutional Report"), 842)
        val txtP = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#0F172A"); textSize = 9.5f }
        val boldP = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#0F172A")
            textSize = 10f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        }

        val headers = report.optJSONArray("headers") ?: JSONArray()
        val colCnt = minOf(headers.length(), 7).coerceAtLeast(1)
        val colW = (842f - margin * 2f) / colCnt
        for (i in 0 until colCnt) {
            c.drawText(headers.optString(i).take(18), margin + i * colW, y, boldP)
        }
        y += 20f

        val rows = report.optJSONArray("rows") ?: JSONArray()
        for (r in 0 until minOf(rows.length(), 20)) {
            val row = rows.getJSONArray(r)
            for (i in 0 until minOf(row.length(), colCnt)) {
                c.drawText(row.optString(i).take(20), margin + i * colW, y, txtP)
            }
            y += 18f
        }

        y += 12f
        c.drawText(report.optString("summaryText"), margin, minOf(y, 565f), boldP)
        doc.finishPage(page)

        val file = File(ensureExportDir(context), "SchoolCMS_Report_${System.currentTimeMillis()}.pdf")
        FileOutputStream(file).use { doc.writeTo(it) }
        doc.close()
        return file
    }

    fun printPdfFile(context: Context, pdfFile: File, jobName: String) {
        val printManager = context.getSystemService(Context.PRINT_SERVICE) as PrintManager
        val adapter = object : PrintDocumentAdapter() {
            override fun onLayout(
                oldAttributes: PrintAttributes?,
                newAttributes: PrintAttributes?,
                cancellationSignal: CancellationSignal?,
                callback: LayoutResultCallback?,
                extras: Bundle?
            ) {
                if (cancellationSignal?.isCanceled == true) {
                    callback?.onLayoutCancelled()
                    return
                }
                val info = PrintDocumentInfo.Builder(pdfFile.name)
                    .setContentType(PrintDocumentInfo.CONTENT_TYPE_DOCUMENT)
                    .build()
                callback?.onLayoutFinished(info, true)
            }

            override fun onWrite(
                pages: Array<out PageRange>?,
                destination: ParcelFileDescriptor?,
                cancellationSignal: CancellationSignal?,
                callback: WriteResultCallback?
            ) {
                try {
                    FileInputStream(pdfFile).use { input ->
                        FileOutputStream(destination!!.fileDescriptor).use { output ->
                            input.copyTo(output)
                        }
                    }
                    callback?.onWriteFinished(arrayOf(PageRange.ALL_PAGES))
                } catch (e: Exception) {
                    callback?.onWriteFailed(e.message)
                }
            }
        }
        printManager.print(jobName, adapter, null)
    }

    fun sharePdfFile(context: Context, pdfFile: File, title: String) {
        val uri = FileProvider.getUriForFile(context, "${context.packageName}.fileprovider", pdfFile)
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "application/pdf"
            putExtra(Intent.EXTRA_STREAM, uri)
            putExtra(Intent.EXTRA_SUBJECT, title)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        context.startActivity(Intent.createChooser(intent, title))
    }
}
