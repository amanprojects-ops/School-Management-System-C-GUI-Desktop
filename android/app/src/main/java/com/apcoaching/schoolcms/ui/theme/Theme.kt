package com.apcoaching.schoolcms.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val BrandBlue = Color(0xFF1E40AF)
val BrandBlueLight = Color(0xFF3B82F6)
val BrandDark = Color(0xFF0F172A)
val BrandTeal = Color(0xFF0D9488)
val StatusGreen = Color(0xFF16A34A)
val StatusRed = Color(0xFFDC2626)
val StatusAmber = Color(0xFFD97706)

private val LightColors = lightColorScheme(
    primary = BrandBlue,
    onPrimary = Color.White,
    secondary = BrandTeal,
    onSecondary = Color.White,
    background = Color(0xFFF1F5F9),
    surface = Color.White,
    onSurface = Color(0xFF0F172A),
    surfaceVariant = Color(0xFFE2E8F0)
)

private val DarkColors = darkColorScheme(
    primary = BrandBlueLight,
    onPrimary = Color.White,
    secondary = Color(0xFF2DD4BF),
    onSecondary = Color(0xFF0F172A),
    background = Color(0xFF0F172A),
    surface = Color(0xFF1E293B),
    onSurface = Color(0xFFF8FAFC),
    surfaceVariant = Color(0xFF334155)
)

@Composable
fun SchoolCmsTheme(
    darkTheme: Boolean = false,
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = if (darkTheme) DarkColors else LightColors,
        content = content
    )
}
