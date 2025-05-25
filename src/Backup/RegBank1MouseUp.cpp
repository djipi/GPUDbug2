void MainWindow::RegBank1MouseUp(QTreeWidgetItem *item, int column)
{
    if (!item)
        return;

    QString text = item->text(0);
    int reg = 0;

    // Extraction du numéro de registre à partir du texte (comme "r0: ..." ou "r12: ...")
    if (text.mid(2, 1) == ":")
        reg = text.mid(1, 1).toInt();
    else
        reg = text.mid(1, 2).toInt();

    QString valueStr = QString("$%1").arg(RegBank[1][reg], 8, 16, QChar('0')).toUpper();

    bool ok;
    QString newValue = QInputDialog::getText(this,
                                             "Register modification",
                                             QString("r%1 =").arg(reg),
                                             QLineEdit::Normal,
                                             valueStr,
                                             &ok);

    if (ok) {
        // Supprime le '$' s'il est présent
        if (newValue.startsWith("$"))
            newValue.remove(0, 1);

        RegBank[1][reg] = newValue.toUInt(nullptr, 16);
        UpdateRegView();
    }
}
