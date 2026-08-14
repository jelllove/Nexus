#include <QtTest>

#include "models/EditorTarget.h"

class SubtaskFeatureTests : public QObject
{
    Q_OBJECT

private slots:
    void editorTargetsKeepLayersDistinct()
    {
        const EditorTarget task = EditorTarget::task(7);
        const EditorTarget subtask = EditorTarget::subtask(7);

        QVERIFY(task.isValid());
        QVERIFY(subtask.isValid());
        QVERIFY(task != subtask);
        QCOMPARE(task.kind, EditorTargetKind::Task);
        QCOMPARE(subtask.kind, EditorTargetKind::SubTask);
        QCOMPARE(task.id, 7);
        QCOMPARE(subtask.id, 7);
        QVERIFY(!EditorTarget().isValid());
    }
};

QTEST_MAIN(SubtaskFeatureTests)
#include "SubtaskFeatureTests.moc"
