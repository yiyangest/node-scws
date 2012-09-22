var scws = require('../lib');

scws = new scws.Scws();

var text = "【桂花糖炒栗子驾到~】秋雨梧桐叶落时，便是杭州最美时~桂花糖炒栗子飘香满街~那股甜香实在太勾引人啦！盛文甘栗、大脚板糖炒栗子、光芒甘栗、林富炒货是比较知名的栗子炒货店；网友爆临平秋山大街有个香栗坊老店很赞！吃货们私藏的那些小巷小店一起来晒晒吧~求扩散~求爆料！";

//scws.segment(text, function(error, results){
    //if (error) {
        //console.log('error: ', error);
    //} else {
        //console.log('results:');
        //console.log(results);
    //}
//});

scws.topwords(text, 10, function(error, results){
    if (error) {
        console.log('error: ', error);
    } else {
        console.log('results: ');
        console.log(results);
    }
});
